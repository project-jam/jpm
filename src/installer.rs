//! Package extraction and installation

use std::path::{Path, PathBuf};
use anyhow::{Result, Context};
use tokio::fs;

use crate::util;

/// Package installer responsible for extracting and installing packages
pub struct PackageInstaller {
    /// Root directory of the project
    project_dir: PathBuf,
}

impl PackageInstaller {
    /// Create a new package installer
    pub fn new<P: AsRef<Path>>(project_dir: P) -> Self {
        Self {
            project_dir: project_dir.as_ref().to_path_buf(),
        }
    }
    
    /// Install a package from a tarball
    pub async fn install_from_tarball<P: AsRef<Path>>(&self, tarball_path: P, package_name: &str) -> Result<()> {
        let node_modules_dir = self.project_dir.join("node_modules");
        let package_dir = node_modules_dir.join(package_name);
        
        // Create node_modules directory if it doesn't exist
        if !node_modules_dir.exists() {
            fs::create_dir_all(&node_modules_dir)
                .await
                .context("Failed to create node_modules directory")?;
        }
        
        // Extract the tarball to the package directory
        // We clone the PathBuf to ensure it lives long enough for the async operation
        util::extract_tarball(tarball_path, package_dir.clone()).await
            .context(format!("Failed to extract package {}", package_name))?;
        
        Ok(())
    }
    
    /// Link bin files from a package
    pub async fn link_binaries(&self, package_name: &str) -> Result<()> {
        let node_modules_dir = self.project_dir.join("node_modules");
        let package_dir = node_modules_dir.join(package_name);
        let bin_dir = node_modules_dir.join(".bin");
        
        // Check if package has a package.json with bin entries
        let package_json_path = package_dir.join("package.json");
        if !package_json_path.exists() {
            // No package.json, nothing to link
            return Ok(());
        }
        
        // Read and parse package.json
        let package_json = fs::read_to_string(&package_json_path)
            .await
            .context("Failed to read package.json")?;
            
        let package_data: serde_json::Value = serde_json::from_str(&package_json)
            .context("Failed to parse package.json")?;
            
        // Check if there are bin entries
        let bin = match package_data.get("bin") {
            Some(bin) => bin,
            None => return Ok(()), // No bin entries
        };
        
        // Create bin directory if it doesn't exist
        if !bin_dir.exists() {
            fs::create_dir_all(&bin_dir)
                .await
                .context("Failed to create bin directory")?;
        }
        
        // Process bin entries based on whether it's a string or an object
        match bin {
            serde_json::Value::String(bin_path) => {
                // Single binary with the same name as the package
                let src = package_dir.join(bin_path);
                let dest = bin_dir.join(package_name);
                
                // Create symlink
                #[cfg(unix)]
                tokio::fs::symlink(&src, &dest)
                    .await
                    .context(format!("Failed to create symlink for {}", package_name))?;
                    
                #[cfg(windows)]
                tokio::fs::symlink_file(&src, &dest)
                    .await
                    .context(format!("Failed to create symlink for {}", package_name))?;
            },
            serde_json::Value::Object(bin_map) => {
                // Multiple binaries
                for (bin_name, bin_path_value) in bin_map {
                    if let serde_json::Value::String(bin_path) = bin_path_value {
                        let src = package_dir.join(bin_path);
                        let dest = bin_dir.join(bin_name);
                        
                        // Create symlink
                        #[cfg(unix)]
                        tokio::fs::symlink(&src, &dest)
                            .await
                            .context(format!("Failed to create symlink for {}", bin_name))?;
                            
                        #[cfg(windows)]
                        tokio::fs::symlink_file(&src, &dest)
                            .await
                            .context(format!("Failed to create symlink for {}", bin_name))?;
                    }
                }
            },
            _ => {}, // Ignore other types
        }
        
        Ok(())
    }
}