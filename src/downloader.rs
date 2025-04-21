//! Network operations for downloading packages

use std::path::Path;
use thiserror::Error;
use tokio::fs::File;
use tokio::io::AsyncWriteExt;

#[derive(Debug, Error)]
pub enum DownloadError {
    #[error("HTTP error: {0}")]
    HttpError(String),
    
    #[error("IO error: {0}")]
    IoError(#[from] std::io::Error),
    
    #[error("Checksum verification failed")]
    ChecksumError,
}

/// Downloader for packages
pub struct Downloader {
    client: reqwest::Client,
}

impl Downloader {
    /// Create a new downloader
    pub fn new() -> Self {
        Self {
            client: reqwest::Client::new(),
        }
    }
    
    /// Download a package tarball to a specified path
    pub async fn download_tarball(&self, url: &str, target_path: &Path, expected_sha: &str) -> Result<(), DownloadError> {
        // Create the request
        let response = self.client.get(url)
            .send()
            .await
            .map_err(|e| DownloadError::HttpError(e.to_string()))?;
            
        if !response.status().is_success() {
            return Err(DownloadError::HttpError(
                format!("Failed to download from {}: {}", url, response.status())
            ));
        }
        
        // Create the target file
        let mut file = File::create(target_path)
            .await
            .map_err(DownloadError::IoError)?;
        
        // Download the content
        let mut content = Vec::new();
        let bytes = response.bytes()
            .await
            .map_err(|e| DownloadError::HttpError(e.to_string()))?;
            
        content.extend_from_slice(&bytes);
        
        // Verify checksum
        let actual_sha = compute_sha256(&content);
        if actual_sha != expected_sha {
            return Err(DownloadError::ChecksumError);
        }
        
        // Write to file
        file.write_all(&content)
            .await
            .map_err(DownloadError::IoError)?;
            
        Ok(())
    }
}

/// Compute SHA256 checksum for data
fn compute_sha256(data: &[u8]) -> String {
    use sha2::{Sha256, Digest};
    
    let mut hasher = Sha256::new();
    hasher.update(data);
    let result = hasher.finalize();
    
    hex::encode(result)
}