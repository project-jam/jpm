use std::time::Duration;

/// Check if a package exists on the npm registry
async fn check_package_exists(name: &str) -> Result<bool, String> {
    let registry_url = format!("https://registry.npmjs.org/{}", name);
    let client = reqwest::Client::new();
    
    match client.head(&registry_url).send().await {
        Ok(response) => Ok(response.status().is_success()),
        Err(e) => Err(format!("Failed to connect to npm registry: {}", e))
    }
}

/// Install a package, checking if it exists on npm first
async fn install_package(pkg_name: &str, version: &str, node_modules_dir: &std::path::Path) -> anyhow::Result<bool> {
    use colored::*;
    use anyhow::Context;
    
    // First check if package exists on npm registry
    match check_package_exists(pkg_name).await {
        Ok(exists) => {
            if !exists {
                eprintln!("  {} Package '{}' not found in npm registry", "!".yellow().bold(), pkg_name);
                return Ok(false);
            }
        },
        Err(e) => {
            eprintln!("  {} {}", "!".yellow().bold(), e);
            // We'll continue anyway for demo purposes
        }
    }
    
    // Show download progress (simulated)
    print!("  {} Downloading... ", "→".cyan());
    tokio::time::sleep(Duration::from_millis(200)).await;
    print!("25% ");
    tokio::time::sleep(Duration::from_millis(200)).await;
    print!("50% ");
    tokio::time::sleep(Duration::from_millis(200)).await;
    print!("75% ");
    tokio::time::sleep(Duration::from_millis(200)).await;
    println!("100%");
    
    // Create package directory
    let pkg_dir = node_modules_dir.join(pkg_name);
    if !pkg_dir.exists() {
        tokio::fs::create_dir_all(&pkg_dir).await
            .context(format!("Failed to create directory for package {}", pkg_name))?;
    }
    
    print!("  {} Extracting... ", "→".cyan());
    tokio::time::sleep(Duration::from_millis(300)).await;
    println!("Done");
    
    // Create a simple package.json
    let pkg_json = format!("{{
  \"name\": \"{}\",
  \"version\": \"{}\",
  \"main\": \"index.js\"\n}}\n", 
        pkg_name, 
        if version == "^1.0.0" { "1.0.0".to_string() } else { version.to_string() }
    );
    
    let pkg_json_path = pkg_dir.join("package.json");
    tokio::fs::write(&pkg_json_path, pkg_json).await
        .context(format!("Failed to write package.json for {}", pkg_name))?;
    
    // Create a simple index.js
    let index_js = format!("console.log('Hello from {}');\n\nmodule.exports = {{ name: '{}' }};\n", pkg_name, pkg_name);
    let index_js_path = pkg_dir.join("index.js");
    tokio::fs::write(&index_js_path, index_js).await
        .context(format!("Failed to write index.js for {}", pkg_name))?;
    
    println!("  {} Installed {}", "✓".green(), pkg_name);
    Ok(true)
}