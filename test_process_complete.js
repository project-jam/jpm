// Complete Process Functionality Test Suite
console.log("=== Complete Process Test Suite ===\n");

// 1. Process Information
console.log("1. Basic Process Information:");
console.log("   Platform:", process.platform);
console.log("   Architecture:", process.arch);
console.log("   Version:", process.version);
console.log("   Process ID:", process.pid);
console.log("   Parent Process ID:", process.ppid);
console.log("   Program name:", process.title);
console.log("\nVersions:");
for (const [key, value] of Object.entries(process.versions)) {
    console.log(`   ${key}: ${value}`);
}
console.log();

// 2. Environment Variables
console.log("2. Environment Variables:");
console.log("   HOME:", process.env.HOME);
console.log("   PATH:", process.env.PATH);
console.log("   SHELL:", process.env.SHELL);
console.log("   USER:", process.env.USER);
console.log("   LANG:", process.env.LANG);
console.log("   JPM_VERSION:", process.env.JPM_VERSION);
console.log("   JPM_PLATFORM:", process.env.JPM_PLATFORM);
console.log();

// 3. Working Directory
console.log("3. Working Directory Operations:");
console.log("   Current working directory:", process.cwd());
console.log("   Attempting to change directory...");
try {
    process.chdir("..");
    console.log("   Changed to parent directory:", process.cwd());
    process.chdir("-");
    console.log("   This should fail (invalid path)");
} catch (error) {
    console.log("   Caught expected error:", error);
}
console.log();

// 4. Process Uptime
console.log("4. Process Uptime Test:");
console.log("   Initial uptime:", process.uptime(), "seconds");
// Small delay to show uptime change
for (let i = 0; i < 1000000; i++) {} // Simple delay
console.log("   Uptime after delay:", process.uptime(), "seconds");
console.log();

// 5. Standard Streams
console.log("5. Standard Streams Test:");
console.log("   5.1. Standard output tests:");
process.stdout.write("      - Direct write to stdout\n");
process.stdout.write("      - Multi-part ");
process.stdout.write("stdout ");
process.stdout.write("write\n");

console.log("   5.2. Standard error tests:");
process.stderr.write("      - Direct write to stderr\n");
process.stderr.write("      - Error simulation: Operation failed\n");

console.log("   5.3. Standard input test:");
console.log("      Please type something and press Enter:");
process.stdin.on('data', (input) => {
    console.log("      Received input:", input);
    console.log("\n   All stream tests completed!");
    console.log("\n=== Test Suite Complete ===");

    // Calculate total runtime
    const runtime = process.uptime();
    console.log(`\nTotal runtime: ${runtime.toFixed(3)} seconds`);

    // Exit with success
    process.exit(0);
});
