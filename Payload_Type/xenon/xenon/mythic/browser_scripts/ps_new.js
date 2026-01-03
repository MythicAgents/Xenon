function(task, responses) {
    if (task.status.includes("error")) {
        const combined = responses.reduce((prev, cur) => prev + cur, "");
        return { 'plaintext': combined };
    } else if (responses.length > 0) {
        // Get all lines (including empty ones) to preserve structure
        const allLines = responses.reduce((prev, cur) => prev + cur, "").split("\n");
        
        // Always preserve the first two lines (prefix from translator)
        // These are: "[+] agent called home, sent: X bytes" and "[+] received output:"
        const prefixLines = [];
        if (allLines.length > 0) {
            prefixLines.push(allLines[0]);
        }
        if (allLines.length > 1) {
            prefixLines.push(allLines[1]);
        }
        
        // Extract data lines (tab-separated values) - skip prefix lines and empty lines
        const dataLines = allLines.slice(2).filter(line => {
            // Data lines should have tabs (processName\tppid\tpid\t...)
            return line.includes("\t");
        });
        
        if (dataLines.length === 0) {
            // If no data lines, return original output
            return { 'plaintext': allLines.join("\n") };
        }

        // Build plaintext output - start with prefix lines
        let outputLines = [];
        
        // Add prefix lines first (always preserve these)
        prefixLines.forEach(line => {
            outputLines.push(line);
        });
        
        // Add empty line after prefix if there are prefix lines
        if (prefixLines.length > 0) {
            outputLines.push("");
        }

        // Column widths for alignment (increased to prevent overflow)
        const processNameWidth = 30;
        const archWidth = 15;
        const pidWidth = 10;
        const ppidWidth = 10;
        const userWidth = 30;
        const sessionWidth = 12;
        
        // Column separator (increased spacing to prevent column overlap)
        const columnSeparator = "      ";  // 6 spaces between columns
        
        // Add column headers
        const headerLine = `Process Name${" ".repeat(processNameWidth - 12)}${columnSeparator}Architecture${" ".repeat(archWidth - 12)}${columnSeparator}PID${" ".repeat(pidWidth - 3)}${columnSeparator}PPID${" ".repeat(ppidWidth - 4)}${columnSeparator}User Account${" ".repeat(userWidth - 12)}${columnSeparator}Session ID`;
        const separatorLine = `${"=".repeat(processNameWidth)}${columnSeparator}${"=".repeat(archWidth)}${columnSeparator}${"=".repeat(pidWidth)}${columnSeparator}${"=".repeat(ppidWidth)}${columnSeparator}${"=".repeat(userWidth)}${columnSeparator}${"=".repeat(sessionWidth)}`;
        
        outputLines.push(headerLine);
        outputLines.push(separatorLine);

        // Format each line with proper column spacing
        // Format: Process Name    Architecture    PID    PPID    User Account    Session ID
        dataLines.forEach(line => {
            const parts = line.split("\t");
            // Process can have 3 or 6 fields depending on whether OpenProcess succeeded
            const processName = parts[0] || "";
            const ppid = parts[1] || "";
            const pid = parts[2] || "";
            const architecture = parts[3] || "";
            const userAccount = parts[4] || "";
            const sessionId = parts[5] || "";
            
            // Truncate values if they exceed column width to prevent overflow
            const truncate = (str, maxLen) => str.length > maxLen ? str.substring(0, maxLen - 3) + "..." : str;
            
            // Format: Process Name    Architecture    PID    PPID    User Account    Session ID
            const formattedLine = `${truncate(processName || "", processNameWidth).padEnd(processNameWidth, " ")}${columnSeparator}${truncate(architecture || "", archWidth).padEnd(archWidth, " ")}${columnSeparator}${(pid || "").padEnd(pidWidth, " ")}${columnSeparator}${(ppid || "").padEnd(ppidWidth, " ")}${columnSeparator}${truncate(userAccount || "", userWidth).padEnd(userWidth, " ")}${columnSeparator}${(sessionId || "").padEnd(sessionWidth, " ")}`;
            outputLines.push(formattedLine);
        });

        return { 'plaintext': outputLines.join("\n") };
    } else {
        return { 'plaintext': "No response yet from agent..." };
    }
}
