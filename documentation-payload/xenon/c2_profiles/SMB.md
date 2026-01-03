+++
title = "smb"
chapter = false
weight = 5
+++

## Summary

This SMB Command and Control (C2) Profile is a Peer-to-Peer (P2P) profile for allowing inter-agent communications. 
The format for messages on the wire is simple. We have `messageA`, a properly formatted base64 Mythic message, that we need to send over the wire in 1KB chunks. Each chunked packet looks like the following:

| Byte Size            | Meaning                                                                              |
|:---------------------|:-------------------------------------------------------------------------------------|
| uint32 (4 Bytes)     | big endian format for the size of this chunk of messageA + the two uint32 after this |
| uint32 (4 Bytes)     | big endian format for the total number of chunks for messageA                        |
| uint32 (4 Bytes)     | big endian format for the current chunk number (starting at 0)                       |
| []byte (variable)    | the raw bytes of this chunk of messageA                                              |

Note: THe 1KB chunk here is just an example. The chunk size can be whatever you want as long as it's under the max size for SMB writes (about 60MB).

If this is the callback that generated `messageA`, then this is the same message that you would send as if you were trying to send out through an egress profile (i.e. base64(uuid+data)).
If you're another callback in a chain that's getting this message then this is either:

* from somebody further away from egress - in which case you should bundle it up in a delegate message and forward it along closer to egress
* from the direction of egress - in which case it's a message for you and you should process it as such

The format of this message structure is meant to be simple enough that most agents should be able to support it, but complex enough that it provides the necessary components for chunking and transferring data.

## Configuration

This profile currently only has a few configuration options:

### pipename

Which pipename the agent should create and listen on. This is currently a bind-only profile. In the future this is likely to change to allow specifying a direction, but for now it's bind-only.

### killdate

When the agent should stop executing.

### encrypted_exchange_check

True or False if this agent should do a key exchange or not.

### AESPSK

Either an aes256 key to use or no encryption at all.

## Authors
- @its_a_feature_
