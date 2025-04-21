use anyhow::Context;
use colored::Colorize;
use flate2::read::GzDecoder;
use serde_json::Value;
use std::io::Cursor;
use std::path::{Path, PathBuf};
use std::process::Command;
use tar::Archive;

/// Install a package, checking if it exists on npm first
pub async fn install_package(
    pkg_name: &str,
    version: &str,
    node_modules_dir: &Path,
) -> anyhow::Result<bool> {
    // Query the npm registry for package metadata
    let registry_url = format!("https://registry.npmjs.org/{}", pkg_name);
    let client = reqwest::Client::new();

    println!("  {} Fetching metadata for {}", "→".cyan(), pkg_name);

    // Send request
    let response = client
        .get(&registry_url)
        .send()
        .await
        .context("Failed to send request to npm registry")?;

    if !response.status().is_success() {
        eprintln!(
            "  {} Package '{}' not found in npm registry",
            "!".yellow().bold(),
            pkg_name
        );
        return Ok(false);
    }

    // Parse JSON
    let package_data: Value = response
        .json()
        .await
        .context("Failed to parse package metadata")?;

    // Resolve version
    let resolved_version = if version == "^1.0.0" || version == "latest" {
        package_data
            .get("dist-tags")
            .and_then(|tags| tags.get("latest"))
            .and_then(|v| v.as_str())
            .map(|v| v.to_string())
            .context("Could not find latest version")?
    } else {
        version.trim_start_matches('^').trim_start_matches('~').to_string()
    };

    println!("  {} Resolved version: {}", "→".cyan(), resolved_version);

    // Get tarball URL
    let tarball_url = package_data
        .get("versions")
        .and_then(|versions| versions.get(&resolved_version))
        .and_then(|vers| vers.get("dist"))
        .and_then(|dist| dist.get("tarball"))
        .and_then(|url| url.as_str())
        .context(format!("Could not find tarball URL for version {}", resolved_version))?;

    // Prepare package directory
    let pkg_dir = node_modules_dir.join(pkg_name);
    if pkg_dir.exists() {
        println!(
            "  {} Package already installed, removing old version",
            "→".cyan()
        );
        tokio::fs::remove_dir_all(&pkg_dir)
            .await
            .context(format!("Failed to remove existing directory for package {}", pkg_name))?;
    }
    tokio::fs::create_dir_all(&pkg_dir)
        .await
        .context(format!("Failed to create directory for package {}", pkg_name))?;

    // Download tarball
    println!("  {} Downloading tarball from {}", "→".cyan(), tarball_url);
    let tarball_response = client
        .get(tarball_url)
        .send()
        .await
        .context("Failed to download tarball")?;
    if !tarball_response.status().is_success() {
        eprintln!(
            "  {} Failed to download tarball: HTTP {}",
            "!".yellow().bold(),
            tarball_response.status()
        );
        return Ok(false);
    }
    let tarball_bytes = tarball_response
        .bytes()
        .await
        .context("Failed to read tarball bytes")?;

    // Extract
    extract_tarball(&tarball_bytes, &pkg_dir).await?;

    println!("  {} Successfully installed {}", "✓".green(), pkg_name);
    Ok(true)
}

/// Extract a gzipped tarball into the target directory, stripping an optional 'package/' prefix
async fn extract_tarball(tarball_bytes: &[u8], target_dir: &Path) -> anyhow::Result<()> {
    println!("  {} Extracting package...", "→".cyan());

    // First pass: detect common prefix
    let mut gz = GzDecoder::new(Cursor::new(tarball_bytes));
    let mut archive = Archive::new(&mut gz);
    let mut paths: Vec<PathBuf> = Vec::new();
    for entry in archive.entries().context("Failed to read archive entries")? {
        let entry = entry.context("Failed to read archive entry")?;
        let path = entry.path().context("Failed to get entry path")?;
        paths.push(path.to_path_buf());
    }
    let common_prefix = if paths.iter().any(|p| p.starts_with("package")) {
        println!("  {} Detected common prefix: 'package/'", "→".cyan());
        Some(PathBuf::from("package"))
    } else {
        None
    };

    // Second pass: extract files
    let mut gz = GzDecoder::new(Cursor::new(tarball_bytes));
    let mut archive = Archive::new(&mut gz);
    for entry in archive.entries().context("Failed to read archive entries")? {
        let mut entry = entry.context("Failed to read archive entry")?;
        let path = entry.path().context("Failed to get entry path")?;
        // Strip prefix if present
        let rel_path = if let Some(ref prefix) = common_prefix {
            path.strip_prefix(prefix).unwrap_or(&path).to_path_buf()
        } else {
            path.to_path_buf()
        };
        let target_path = target_dir.join(&rel_path);
        if let Some(parent) = target_path.parent() {
            tokio::fs::create_dir_all(parent)
                .await
                .context("Failed to create directory")?;
        }
        entry.unpack(&target_path).context("Failed to unpack entry")?;
        println!("  {} Extracted {}", "✓".green(), rel_path.display());
    }

    // Post-extraction: run any install/build scripts
    let pkg_json_path = target_dir.join("package.json");
    if pkg_json_path.exists() {
        let content = tokio::fs::read_to_string(&pkg_json_path)
            .await
            .context("Failed to read package.json")?;
        let pkg_json: Value = serde_json::from_str(&content).context("Invalid package.json")?;
        if let Some(scripts) = pkg_json.get("scripts").and_then(Value::as_object) {
            if let Some(install_script) = scripts.get("install").and_then(Value::as_str) {
                println!("  {} Running install script: {}", "→".cyan(), install_script);
                if let Err(e) = Command::new("npm")
                    .current_dir(target_dir)
                    .arg("run")
                    .arg("install")
                    .status()
                {
                    eprintln!("  {} Failed to run install script: {}", "!".yellow().bold(), e);
                }
            }
            if let Some(build_script) = scripts.get("build").and_then(Value::as_str) {
                println!("  {} Running build script: {}", "→".cyan(), build_script);
                if let Err(e) = Command::new("npm")
                    .current_dir(target_dir)
                    .arg("run")
                    .arg("build")
                    .status()
                {
                    eprintln!("  {} Failed to run build script: {}", "!".yellow().bold(), e);
                }
            }
        }
    }

    Ok(())
}
