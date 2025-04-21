use anyhow::Context;
use clap::{Parser, Subcommand};
use colored::Colorize;
use dialoguer::{Confirm, Input};
use flate2::read::GzDecoder;
use indicatif::ProgressStyle;
use serde_json::{json, Value};
use std::io::{Cursor, Read};
use std::path::{Path, PathBuf};
use std::time::Duration;
use tar::Archive;
use tokio;

#[derive(Parser)]
#[command(author, version, about, long_about = None)]
struct Cli {
    #[command(subcommand)]
    command: Option<Commands>,
}

#[derive(Subcommand)]
enum Commands {
    Init,
    Install {
        #[arg(value_parser)]
        packages: Vec<String>,
    },
}

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    let cli = Cli::parse();

    match cli.command {
        Some(Commands::Init) => {
            init_package().await?;
        }
        Some(Commands::Install { packages }) => {
            install_packages(&packages).await?;
        }
        None => {
            println!("No command specified. Use --help for usage information.");
        }
    }

    Ok(())
}

async fn init_package() -> anyhow::Result<()> {
    println!("Initializing new package...");

    let name: String = Input::new()
        .with_prompt("What is the name of the project?")
        .interact()?;

    let license: String = Input::new()
        .with_prompt("What's the license?")
        .default("MIT".into())
        .interact()?;

    let description: String = Input::new()
        .with_prompt("What's the description? (optional)")
        .allow_empty(true)
        .interact()?;

    let add_test = Confirm::new()
        .with_prompt("Do you want to add a test command?")
        .default(false)
        .interact()?;

    let packages: String = Input::new()
        .with_prompt("What are the packages that you need to install? (separated by space, can use package@version format)")
        .allow_empty(true)
        .interact()?;

    // Create package.json
    let mut package_json = json!({
        "name": name,
        "version": "1.0.0",
        "description": description,
        "license": license,
        "dependencies": {}
    });

    if add_test {
        package_json["scripts"] = json!({
            "test": "echo \"Error: no test specified\" && exit 1"
        });
    }

    tokio::fs::write("package.json", serde_json::to_string_pretty(&package_json)?).await?;

    println!("{} Created package.json with name: {}", "✓".green(), name);

    if !packages.trim().is_empty() {
        println!("Installing dependencies...");
        let packages: Vec<String> = packages.split_whitespace().map(String::from).collect();
        install_packages(&packages).await?;
    }

    Ok(())
}

async fn install_packages(packages: &[String]) -> anyhow::Result<()> {
    let node_modules = PathBuf::from("node_modules");
    if !node_modules.exists() {
        tokio::fs::create_dir_all(&node_modules).await?;
    }

    // Load or create package.json
    let package_json_path = "package.json";
    let mut package_json = if Path::new(package_json_path).exists() {
        let content = tokio::fs::read_to_string(package_json_path).await?;
        serde_json::from_str(&content)?
    } else {
        json!({
            "name": "my-project",
            "version": "1.0.0",
            "description": "",
            "license": "MIT",
            "dependencies": {}
        })
    };

    let deps = package_json
        .get_mut("dependencies")
        .and_then(|v| v.as_object_mut())
        .ok_or_else(|| anyhow::anyhow!("Invalid package.json structure"))?;

    // Keep track of all packages we need to install
    let mut pending_packages: Vec<(String, String)> = Vec::new();
    let installed_packages =
        std::sync::Arc::new(tokio::sync::Mutex::new(std::collections::HashSet::new()));
    let deps_lock = std::sync::Arc::new(tokio::sync::Mutex::new(deps.clone()));

    // Add initial packages to pending list
    for package_spec in packages {
        let (name, version) = if let Some(idx) = package_spec.find('@') {
            let (n, v) = package_spec.split_at(idx);
            (n.to_string(), v[1..].to_string())
        } else {
            (package_spec.to_string(), "latest".to_string())
        };
        pending_packages.push((name, version));
    }

    // Process packages in parallel with a semaphore to limit concurrency
    let semaphore = std::sync::Arc::new(tokio::sync::Semaphore::new(8)); // Max 8 concurrent installations
    let mut tasks = Vec::new();

    while let Some((name, version)) = pending_packages.pop() {
        let installed = installed_packages.clone();
        let node_modules = node_modules.clone();
        let semaphore = semaphore.clone();
        let deps_lock = deps_lock.clone();

        tasks.push(tokio::spawn(async move {
            let _permit = semaphore.acquire().await.unwrap();

            // Skip if already installed
            {
                let installed = installed.lock().await;
                if installed.contains(&name) {
                    return Ok::<Vec<(String, String)>, anyhow::Error>(Vec::new());
                }
            }

            // Query npm registry for package info
            let registry_url = format!("https://registry.npmjs.org/{}", name);
            let client = reqwest::Client::new();
            let response = client.get(&registry_url).send().await?;

            if response.status().is_success() {
                let pkg_data: Value = response.json().await?;
                let exact_version = if version == "latest" {
                    pkg_data
                        .get("dist-tags")
                        .and_then(|t| t.get("latest"))
                        .and_then(|v| v.as_str())
                        .unwrap_or(&version)
                } else {
                    &version
                };

                // Handle scoped packages directory structure
                let install_dir = if name.starts_with('@') {
                    let parts: Vec<&str> = name.split('/').collect();
                    if parts.len() == 2 {
                        node_modules.join(parts[0]).join(parts[1])
                    } else {
                        node_modules.join(&name)
                    }
                } else {
                    node_modules.join(&name)
                };

                // Download and install package
                if let Ok(true) = download_and_install(&name, exact_version, &install_dir).await {
                    // Update dependencies in package.json
                    {
                        let mut deps = deps_lock.lock().await;
                        deps.insert(name.clone(), json!(exact_version));
                    }

                    {
                        let mut installed = installed.lock().await;
                        installed.insert(name.clone());
                    }

                    // Read package's dependencies
                    let mut new_deps = Vec::new();
                    let pkg_json_path = install_dir.join("package.json");
                    if pkg_json_path.exists() {
                        let pkg_content = tokio::fs::read_to_string(&pkg_json_path).await?;
                        if let Ok(pkg_manifest) = serde_json::from_str::<Value>(&pkg_content) {
                            if let Some(pkg_deps) =
                                pkg_manifest.get("dependencies").and_then(|d| d.as_object())
                            {
                                for (dep_name, dep_version) in pkg_deps {
                                    let version_str = dep_version
                                        .as_str()
                                        .unwrap_or("latest")
                                        .trim_start_matches('^')
                                        .trim_start_matches('~')
                                        .to_string();
                                    new_deps.push((dep_name.clone(), version_str));
                                }
                            }
                        }
                    }
                    return Ok(new_deps);
                }
            }
            Ok::<Vec<(String, String)>, anyhow::Error>(Vec::new())
        }));

        // Wait for some tasks to complete to get new dependencies
        if tasks.len() >= 8 {
            if let Some(task) = futures_util::future::select_all(&mut tasks).await.0.ok() {
                if let Ok(new_deps) = task {
                    pending_packages.extend(new_deps);
                }
            }
        }
    }

    // Wait for remaining tasks
    for task in tasks {
        if let Ok(Ok(new_deps)) = task.await {
            pending_packages.extend(new_deps);
        }
    }

    // Get final dependencies
    let final_deps = deps_lock.lock().await;

    // Sort dependencies alphabetically
    let mut sorted_deps: Vec<_> = final_deps.iter().collect();
    sorted_deps.sort_by(|a, b| a.0.cmp(b.0));

    // Create new sorted object
    let sorted_obj =
        serde_json::Map::from_iter(sorted_deps.into_iter().map(|(k, v)| (k.clone(), v.clone())));
    package_json["dependencies"] = Value::Object(sorted_obj);

    // Format package.json with specific order and indentation
    let formatted = json!({
        "name": package_json["name"],
        "version": package_json["version"],
        "description": package_json.get("description").unwrap_or(&json!("")),
        "license": package_json.get("license").unwrap_or(&json!("MIT")),
        "dependencies": package_json["dependencies"],
    });

    // Write updated package.json with consistent formatting
    tokio::fs::write(
        package_json_path,
        serde_json::to_string_pretty(&formatted)? + "\n",
    )
    .await?;

    Ok(())
}

async fn download_and_install(
    pkg_name: &str,
    version: &str,
    install_dir: &Path,
) -> anyhow::Result<bool> {
    // Query npm registry
    let registry_url = format!("https://registry.npmjs.org/{}", pkg_name);
    let client = reqwest::Client::new();

    println!("  {} Fetching metadata for {}", "→".cyan(), pkg_name);
    let response = client.get(&registry_url).send().await?;

    if !response.status().is_success() {
        return Ok(false);
    }

    let package_data: Value = response.json().await?;

    let tarball_url = package_data
        .get("versions")
        .and_then(|v| v.get(version))
        .and_then(|v| v.get("dist"))
        .and_then(|d| d.get("tarball"))
        .and_then(|u| u.as_str())
        .context("Could not find tarball URL")?;

    // Prepare package directory
    if install_dir.exists() {
        tokio::fs::remove_dir_all(&install_dir).await?;
    }
    tokio::fs::create_dir_all(&install_dir).await?;

    // Download tarball
    println!("  {} Downloading tarball", "→".cyan());
    let bytes = client.get(tarball_url).send().await?.bytes().await?;

    // Extract in a blocking task to avoid thread safety issues with tar
    let install_dir = install_dir.to_path_buf();
    let bytes = bytes.to_vec();
    tokio::task::spawn_blocking(move || -> anyhow::Result<()> {
        let gz = GzDecoder::new(Cursor::new(&bytes));
        let mut archive = Archive::new(gz);

        // First detect common prefix
        let entries = archive.entries()?;
        let has_prefix = entries
            .filter_map(|e| e.ok())
            .any(|e| e.path().map(|p| p.starts_with("package/")).unwrap_or(false));

        // Then extract
        let gz = GzDecoder::new(Cursor::new(&bytes));
        let mut archive = Archive::new(gz);
        for entry in archive.entries()? {
            let mut entry = entry?;
            let path = entry.path()?.into_owned();
            let rel = if has_prefix {
                path.strip_prefix("package").unwrap_or(&path).to_path_buf()
            } else {
                path
            };
            let dest = install_dir.join(&rel);
            if let Some(parent) = dest.parent() {
                std::fs::create_dir_all(parent)?;
            }
            entry.unpack(&dest)?;
        }
        Ok(())
    })
    .await??;

    println!("  {} Successfully installed {}", "✓".green(), pkg_name);
    Ok(true)
}
