//! Utility functions for the package manager

use std::path::{Path, PathBuf};
use anyhow::{Result, Context};
use tokio::fs;
use std::env;

/// Get the cache directory for JPM
pub async fn get_cache_dir() -> Result<PathBuf> {
    // Try to use standard cache directories by platform
    #[cfg(target_os = "linux")]
    let base_cache_dir = env::var("XDG_CACHE_HOME")
        .map(PathBuf::from)
        .unwrap_or_else(|_| {
            let home = env::var("HOME").expect("HOME environment variable not set");
            PathBuf::from(home).join(".cache")
        });

    #[cfg(target_os = "macos")]
    let base_cache_dir = {
        let home = env::var("HOME").expect("HOME environment variable not set");
        PathBuf::from(home).join("Library").join("Caches")
    };

    #[cfg(target_os = "windows")]
    let base_cache_dir = {
        let appdata = env::var("LOCALAPPDATA").expect("LOCALAPPDATA environment variable not set");
        PathBuf::from(appdata)
    };

    #[cfg(not(any(target_os = "linux", target_os = "macos", target_os = "windows")))]
    let base_cache_dir = {
        let home = env::var("HOME").expect("HOME environment variable not set");
        PathBuf::from(home).join(".cache")
    };

    let cache_dir = base_cache_dir.join("jpm");

    // Create the cache directory if it doesn't exist
    if !cache_dir.exists() {
        fs::create_dir_all(&cache_dir)
            .await
            .context("Failed to create cache directory")?;
    }

    Ok(cache_dir)
}

/// Get the package cache directory
pub async fn get_package_cache_dir() -> Result<PathBuf> {
    let cache_dir = get_cache_dir().await?;
    let package_dir = cache_dir.join("packages");

    // Create the package cache directory if it doesn't exist
    if !package_dir.exists() {
        fs::create_dir_all(&package_dir)
            .await
            .context("Failed to create package cache directory")?;
    }

    Ok(package_dir)
}

/// Extract a tarball to a specific directory
pub async fn extract_tarball<P, Q>(tarball_path: P, target_dir: Q) -> Result<()>
where
    P: AsRef<Path>,
    Q: AsRef<Path> + Send + 'static,
{
    let tarball_path = tarball_path.as_ref().to_path_buf();
    let target_dir = target_dir.as_ref().to_path_buf();

    // Ensure the target directory exists
    if !target_dir.exists() {
        fs::create_dir_all(&target_dir)
            .await
            .context("Failed to create target directory")?;
    }

    // Read the tarball file
    let file = fs::File::open(&tarball_path)
        .await
        .context("Failed to open tarball")?;
    
    // Convert to std::fs::File for compatibility with the tar crate
    let file = file.into_std().await;
    
    // Extract the tarball
    let decoder = flate2::read::GzDecoder::new(file);
    let mut archive = tar::Archive::new(decoder);
    
    // Use a thread to avoid blocking the async runtime
    tokio::task::spawn_blocking(move || -> Result<()> {
        archive.unpack(target_dir)
            .context("Failed to extract tarball")
    })
    .await
    .context("Failed to complete extraction task")?
    .context("Failed to extract tarball")?;

    Ok(())
}

/// Get normalized package name (handle scoped packages)
pub fn normalize_package_name(name: &str) -> String {
    // Replace slashes in scoped packages with appropriate encoding
    if name.starts_with('@') {
        name.replace('/', "%2f")
    } else {
        name.to_string()
    }
}