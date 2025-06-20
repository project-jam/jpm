// Advanced Process Features Test
console.log("=== Advanced Process Features Test ===\n");

// 1. Process Events
console.log("1. Testing Process Events:");
console.log("   Setting up event handlers...");

let exitHandled = false;
process.on('exit', (code) => {
    exitHandled = true;
    console.log("   'exit' event received with code:", code);
});

process.on('warning', (warning) => {
    console.log("   Warning event received:", warning);
});

// Custom event test
console.log("   Testing custom events...");
process.emit('warning', 'This is a test warning');

// 2. High Resolution Time
console.log("\n2. Testing process.hrtime():");
const start = process.hrtime();

// Do some work
let sum = 0;
for (let i = 0; i < 1000000; i++) {
    sum += i;
}

const diff = process.hrtime(start);
console.log("   Operation took:", diff[0], "seconds and", diff[1], "nanoseconds");

// 3. Next Tick Queue
console.log("\n3. Testing process.nextTick():");
console.log("   Adding callbacks to next tick queue...");

let tickOrder = [];

// Add several nextTick callbacks
process.nextTick(() => {
    tickOrder.push(1);
    console.log("   First nextTick executed");
});

process.nextTick(() => {
    tickOrder.push(2);
    console.log("   Second nextTick executed");

    process.nextTick(() => {
        tickOrder.push(4);
        console.log("   Nested nextTick executed");
    });
});

process.nextTick(() => {
    tickOrder.push(3);
    console.log("   Third nextTick executed");
});

// 4. Process Memory Usage Over Time
console.log("\n4. Memory Usage Monitoring:");

function formatBytes(bytes) {
    const units = ['B', 'KB', 'MB', 'GB'];
    let size = bytes;
    let unitIndex = 0;

    while (size >= 1024 && unitIndex < units.length - 1) {
        size /= 1024;
        unitIndex++;
    }

    return `${size.toFixed(2)} ${units[unitIndex]}`;
}

// Monitor memory at intervals
let memoryReadings = [];
for (let i = 0; i < 3; i++) {
    const memory = process.memoryUsage();
    memoryReadings.push({
        timestamp: process.uptime(),
        rss: memory.rss,
        heapTotal: memory.heapTotal,
        heapUsed: memory.heapUsed
    });

    console.log(`   Reading ${i + 1}:`);
    console.log(`     Uptime: ${memoryReadings[i].timestamp.toFixed(3)}s`);
    console.log(`     RSS: ${formatBytes(memory.rss)}`);
    console.log(`     Heap Total: ${formatBytes(memory.heapTotal)}`);
    console.log(`     Heap Used: ${formatBytes(memory.heapUsed)}`);

    // Create some memory pressure
    let arr = new Array(10000).fill('test');
    arr = null; // Allow for garbage collection
}

// 5. Process Information
console.log("\n5. Process Information:");
console.log("   PID:", process.pid);
console.log("   Parent PID:", process.ppid);
console.log("   Platform:", process.platform);
console.log("   Architecture:", process.arch);
console.log("   Version:", process.version);
console.log("   Current Directory:", process.cwd());

// 6. Environment Variables
console.log("\n6. Environment Variables:");
console.log("   HOME:", process.env.HOME);
console.log("   PATH:", process.env.PATH?.substring(0, 50) + "...");
console.log("   SHELL:", process.env.SHELL);
console.log("   JPM_VERSION:", process.env.JPM_VERSION);
console.log("   JPM_PLATFORM:", process.env.JPM_PLATFORM);

// Verify nextTick execution order
process.nextTick(() => {
    console.log("\n7. NextTick Execution Order:", tickOrder);
    console.log("   Expected order: [1, 2, 3, 4]");

    // Final status
    console.log("\n=== Test Complete ===");
    console.log("Total test duration:", process.uptime().toFixed(3), "seconds");

    // Trigger exit event
    process.exit(0);
});
