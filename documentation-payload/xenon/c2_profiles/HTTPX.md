+++
title = "httpx"
chapter = false
weight = 5
+++

## Overview
This C2 profile consists of HTTP requests from an agent to the C2 profile container, where messages are then forwarded to Mythic's API.
Agent messages and server responses can go through a series of transformations to make them blend into traffic.

Agent configurations are defined in either JSON or TOML files and uploaded as part of the payload creation process.
When building a payload, these files are sent to the HTTPX container and processed for potential configuration issues.
It's during this time that the configuration is saved locally so that it can be ingested when the server starts.

## Unique Features

### Multiple Callback Domains
This C2 Profile allows you to specify multiple callback domains, such as: `["https://redirector1.com", "https://redirector2.com:8443"]`.

### Domain Rotation
With multiple callback domains, you need to specify some way of rotating between them. This profile currently offers two options:
* `round-robin` - each request goes to the next domain in the list and circles back around
* `fail-over` - the first domain is used until it hits a certain number of failed messages, then it moves to the next one

### Agent Configuration
Your agent configuration can be specified via either TOML or JSON files. These are uploaded with a payload's build and have their own `name`.
The `name` of this specification is how the C2 server keeps track of all the variations and makes sure that GET/POST requests are associated properly.

Full examples in JSON and TOML can be found on the examples sub-page.

#### Transforms

The agent configuration allows you to specify a series of transforms for what happens when a client sends a message to the server and when the server responds.
The current set of transforms is as follows:

* base64 / base64url
* netbios / netbiosu
* append / prepend
* xor

#### Message Location

The last thing you have to configure as part of your agent configuration, and the most important, is the location of the message. This can be one of the following:

* cookie
* query
* header
* body

If you specify `cookie`, `query`, or `header`, then you also need to specify a `name` to go along with it.

#### Headers

You can also specify for the client and server messages any specific headers you want to set and their values

#### Parameters

For client messages, you can also specify a set of query parameters to include.

### C2 Workflow
{{<mermaid>}}
sequenceDiagram
participant M as Mythic
participant H as HTTP Container
participant A as Agent
A ->>+ H: GET/POST for tasking
H ->>+ M: forward request to Mythic
M -->>- H: reply with tasking
H -->>- A: reply with tasking
{{< /mermaid >}}
Legend:

- Solid line is a new connection
- Dotted line is a message within that connection

## Configuration Options
The profile reads a `config.json` file for a set of `Gin` (Golang) webservers to stand up (`80` by default) and redirects the content.

```JSON
{
  "instances": [
  {
    "ServerHeaders": {
      "Server": "NetDNA-cache/2.2",
      "Cache-Control": "max-age=0, no-cache",
      "Pragma": "no-cache",
      "Connection": "keep-alive",
      "Content-Type": "application/javascript; charset=utf-8"
    },
    "port": 80,
    "key_path": "privkey.pem",
    "cert_path": "fullchain.pem",
    "debug": true,
    "use_ssl": false,
    "bind_ip": "0.0.0.0"
    }
  ]
}
```

A note about debugging:
- With `debug` set to `true`, you'll be able to `view stdout/stderr` from within the UI for the container, but it's not recommended to always have this on (especially if you start using something like SOCKS). There can be a lot of traffic and a lot of debugging information captured here which can be both a performance and memory bottleneck depending on your environment and operational timelines.
- It's recommended to have it on initially to help troubleshoot payload connectivity and configuration issues, but then to set it to `false` for actual operations

### Profile Options
#### crypto type
Indicate if you want to use no crypto (i.e. plaintext) or if you want to use Mythic's aes256_hmac. Using no crypto is really helpful for agent development so that it's easier to see messages and get started faster, but for actual operations you should leave the default to aes256_hmac.

#### Callback Domains
A series of domains (with protocol and port if necessary) to use when connecting back to Mythic. If you're using redirector(s), put those domains here.

#### Callback Interval
A number to indicate how many seconds the agent should wait in between tasking requests.

#### Callback Jitter
Percentage of jitter effect for callback interval.

#### Kill Date
Date for the agent to automatically exit, typically the after an assessment is finished.

#### Perform Key Exchange
True or False for if you want to perform a key exchange with the Mythic Server. When this is true, the agent uses the key specified by the base64 32Byte key to send an initial message to the Mythic server with a newly generated RSA public key. If this is set to `F`, then the agent tries to just use the base64 of the key as a static AES key for encryption. If that key is also blanked out, then the requests will all be in plaintext.

#### Domain Rotation
This indicates how you want your domains to be used (only really matters if you specify more than one domain). `fail-over` will use the first domain until it fails `failover_threshold` times, then it moves to the next one. `round-robin` will just keep using the next one in sequence for each message.


## OPSEC

This profile doesn't do any randomization of network components outside of callback_interval/callback_jitter and callback_domains.