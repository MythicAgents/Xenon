+++
title = "job_kill"
chapter = false
weight = 103
hidden = false
+++

## Summary
Cancel a queued or in-progress file upload task. The agent walks the upload queue, removes the matching entry, closes the file handle, deletes any partially-written file on disk, and reports an error for the cancelled task.

### Arguments

#### task uuid
The Mythic task UUID of the upload task to cancel.

## Usage
```
job_kill <task-uuid>
```

Example
```
job_kill a1b2c3d4-1234-5678-abcd-ef0123456789
```
