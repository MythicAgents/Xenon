+++
title = "sleep"
chapter = false
weight = 103
hidden = false
+++

## Summary
Change the agent's callback interval and optional jitter percentage.

### Arguments

#### seconds
The number of seconds to sleep between check-ins.

#### jitter (optional)
A jitter percentage (0–99) to randomize the sleep interval. For example, a sleep of `60` with jitter `25` will sleep between 45 and 75 seconds.

## Usage
```
sleep <seconds> [jitter]
```

Example
```
sleep 60
sleep 60 25
```
