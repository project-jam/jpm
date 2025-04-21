//! Package management and manifest handling

use std::collections::HashMap;
use std::path::{Path, PathBuf};
use serde::{Serialize, Deserialize};
use thiserror::Error;
use tokio::fs;
use tokio::io::AsyncWriteExt;

#[derive(Debug, Error)]
pub enum PackageError {
    #[error("IO error: {0}")]
    IoError(#[from] std::io::Error),
    
    #[error("JSON parsing error: {0}")]
    JsonError(#[from] serde_json::Error),
    
    #[error("Package manifest not found")]
    ManifestNotFound,
    
    #[error("Invalid package manifest: {0}")]
    InvalidManifest(String),
}

/// Package manifest (similar to package.json)
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PackageManifest {
    pub name: String,
    pub version: String,
    #[serde(default)]
    pub description: Option<String>,
    #[serde(default)]
    pub author: Option<String>,
    #[serde(default)]
    pub main: Option<String>,
    #[serde(default)]
    pub dependencies: HashMap<String, String>,
    #[serde(default)]
    pub dev_dependencies: HashMap<String, String>,
    #[serde(flatten)]
    pub extra: HashMap<String, serde_json::Value>,
}

impl Default for PackageManifest {
    fn default() -> Self {
        Self {
            name: String::new(),
            version: String::from("0.1.0"),
            description: None,
            author: None,
            main: None,
            dependencies: HashMap::new(),
            dev_dependencies: HashMap::new(),
            extra: HashMap::new(),
        }
    }
}

/// Lock file (similar to package-lock.json)
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LockFile {
    pub version: String,
    pub packages: HashMap<String, LockPackage>,
}

/// Package entry in the lock file
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LockPackage {
    pub version: String,
    pub resolved: String,
    pub integrity: String,
    #[serde(default)]
    pub dependencies: HashMap<String, String>,
}

/// Package manager for handling manifests and lock files
pub struct PackageManager {
    root_dir: PathBuf,
}

impl PackageManager {
    /// Create a new package manager with the given root directory
    pub fn new<P: AsRef<Path>>(root_dir: P) -> Self {
        Self {
            root_dir: root_dir.as_ref().to_path_buf(),
        }
    }
    
    /// Load package manifest from the root directory
    pub async fn load_manifest(&self) -> Result<PackageManifest, PackageError> {
        let manifest_path = self.root_dir.join("package.json");
        
        if !manifest_path.exists() {
            return Err(PackageError::ManifestNotFound);
        }
        
        let content = fs::read_to_string(&manifest_path).await?;
        let manifest: PackageManifest = serde_json::from_str(&content)?;
        
        Ok(manifest)
    }
    
    /// Save package manifest to the root directory
    pub async fn save_manifest(&self, manifest: &PackageManifest) -> Result<(), PackageError> {
        let manifest_path = self.root_dir.join("package.json");
        let content = serde_json::to_string_pretty(manifest)?;
        
        let mut file = fs::File::create(manifest_path).await?;
        file.write_all(content.as_bytes()).await?;
        
        Ok(())
    }
    
    /// Load lock file from the root directory
    pub async fn load_lock_file(&self) -> Result<LockFile, PackageError> {
        let lock_path = self.root_dir.join("jpm-lock.json");
        
        if !lock_path.exists() {
            return Err(PackageError::ManifestNotFound);
        }
        
        let content = fs::read_to_string(&lock_path).await?;
        let lock_file: LockFile = serde_json::from_str(&content)?;
        
        Ok(lock_file)
    }
    
    /// Save lock file to the root directory
    pub async fn save_lock_file(&self, lock_file: &LockFile) -> Result<(), PackageError> {
        let lock_path = self.root_dir.join("jpm-lock.json");
        let content = serde_json::to_string_pretty(lock_file)?;
        
        let mut file = fs::File::create(lock_path).await?;
        file.write_all(content.as_bytes()).await?;
        
        Ok(())
    }
    
    /// Initialize a new package in the current directory
    pub async fn init_package(&self) -> Result<PackageManifest, PackageError> {
        // Create a default package manifest
        let dir_name = self.root_dir
            .file_name()
            .and_then(|name| name.to_str())
            .unwrap_or("my-package")
            .to_string();
            
        let manifest = PackageManifest {
            name: dir_name,
            ..Default::default()
        };
        
        // Save the manifest
        self.save_manifest(&manifest).await?;
        
        Ok(manifest)
    }
    
    /// Add a dependency to the package manifest
    pub async fn add_dependency(&self, name: &str, version: &str, is_dev: bool) -> Result<(), PackageError> {
        let mut manifest = match self.load_manifest().await {
            Ok(manifest) => manifest,
            Err(PackageError::ManifestNotFound) => {
                return Err(PackageError::ManifestNotFound);
            }
            Err(e) => return Err(e),
        };
        
        if is_dev {
            manifest.dev_dependencies.insert(name.to_string(), version.to_string());
        } else {
            manifest.dependencies.insert(name.to_string(), version.to_string());
        }
        
        self.save_manifest(&manifest).await
    }
    
    /// Remove a dependency from the package manifest
    pub async fn remove_dependency(&self, name: &str) -> Result<bool, PackageError> {
        let mut manifest = self.load_manifest().await?;
        
        let removed_from_deps = manifest.dependencies.remove(name).is_some();
        let removed_from_dev_deps = manifest.dev_dependencies.remove(name).is_some();
        
        if removed_from_deps || removed_from_dev_deps {
            self.save_manifest(&manifest).await?;
            Ok(true)
        } else {
            Ok(false)
        }
    }
}