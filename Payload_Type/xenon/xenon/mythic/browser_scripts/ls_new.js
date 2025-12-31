function(task, responses) {
    if (task.status.includes("error")) {
        const combined = responses.reduce((prev, cur) => prev + cur, "");
        return { 'plaintext': combined };
    } else if (responses.length > 0) {
        // Parse the output data
        const output = responses.reduce((prev, cur) => prev + cur, "").split("\n").filter(line => line.trim() !== "");
        if (output.length <= 2) {
            return { 'plaintext': "No directory contents found." };
        }

        // Extract path and rows
        const pathLine = output[1];
        const dataLines = output.slice(2);

        // Format date from MM/DD/YY HH:MM:SS to MM/DD/YYYY HH:MM AM/PM
        function formatDate(dateTimeStr) {
            const parts = dateTimeStr.split(" ");
            if (parts.length !== 2) return dateTimeStr;
            
            const [datePart, timePart] = parts;
            const [month, day, year] = datePart.split("/");
            const [hour, minute, second] = timePart.split(":");
            
            // Convert 2-digit year to 4-digit (assuming 2000s)
            const fullYear = year.length === 2 ? "20" + year : year;
            
            // Convert 24-hour to 12-hour format
            let hour24 = parseInt(hour);
            const ampm = hour24 >= 12 ? "PM" : "AM";
            hour24 = hour24 % 12;
            if (hour24 === 0) hour24 = 12;
            
            return `${month.padStart(2, "0")}/${day.padStart(2, "0")}/${fullYear}  ${hour24.toString().padStart(2, "0")}:${minute.padStart(2, "0")} ${ampm}`;
        }

        // Format size - convert bytes to b/kb/mb/gb format (only for files)
        function formatSize(sizeStr) {
            const size = parseInt(sizeStr);
            if (isNaN(size)) return sizeStr;
            
            let formatted;
            if (size < 1024) {
                formatted = size + "b";
            } else if (size < 1024 * 1024) {
                formatted = Math.round(size / 1024) + "kb";
            } else if (size < 1024 * 1024 * 1024) {
                formatted = Math.round(size / (1024 * 1024)) + "mb";
            } else {
                formatted = Math.round(size / (1024 * 1024 * 1024)) + "gb";
            }
            
            return formatted;
        }

        // Build plaintext output like Cobalt Strike (no table borders)
        let outputLines = [];
        outputLines.push(`Contents of ${pathLine}`);
        outputLines.push("");

        // Format each line with proper column spacing
        // Format: Date/Time    <TYPE>    Size    Name
        // Example: 03/31/2025  12:32 PM    <FILE>    295kb       azure-groups.txt
        // Example: 10/23/2024  12:33 PM    <DIR>                     Contacts
        dataLines.forEach(line => {
            const parts = line.split("\t");
            if (parts.length === 4) {
                const [type, size, modified, name] = parts;
                const dateStr = formatDate(modified);
                const typeStr = type === "D" ? "<DIR>" : "<FILE>";
                const sizeStr = type === "D" ? "" : formatSize(size);
                
                // Column widths and spacing
                const dateWidth = 20;  // MM/DD/YYYY  HH:MM AM/PM
                const typeWidth = 6;   // <FILE> or <DIR>
                const sizeWidth = 12;  // Right-aligned size column (e.g., "295kb")
                
                // Format the line: Date    Type    Size    Name
                const paddedSize = sizeStr.padStart(sizeWidth, " ");
                outputLines.push(`${dateStr.padEnd(dateWidth, " ")}    ${typeStr.padEnd(typeWidth, " ")}    ${paddedSize}       ${name}`);
            }
        });

        return { 'plaintext': outputLines.join("\n") };
    } else {
        return { 'plaintext': "No response yet from agent..." };
    }
}
