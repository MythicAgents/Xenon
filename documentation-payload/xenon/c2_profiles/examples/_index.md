+++
title = "examples"
chapter = false
weight = 5
+++

# Examples

## Random Example

This is just a random example in TOML that breaks down all the options and what they mean:

```toml
name = "TEST"
################################################
## HTTP GET
################################################
## Description:
##      GET is used to poll teamserver for tasks
## Defaults:
##    verb "GET" <-- "GET" or "POST"
##    uris ["/activity"] <-- array of strings randomly chosen each time
[get]
verb = "GET"
uris = ["/my/uri/path"]

################################################
## CLIENT
################################################
## Description:
##      client identifies data going from the agent to the server
## Defaults:
##    headers <-- dictionary of key/value pairs
##      set with client.headers.Key = value
##    domain_specific_headers <-- dictionary of dictionaries of key/value pairs
##          Allows setting headers specific to callback hosts
##      set with client.domain_specific_headers."domain".key = value
##    parameters <-- dictionary of key/value pairs
##      set with client.parameters.Key = value
##    message <-- dictionary of key/value pairs about the message the agent is sending to the server
##      location "cookie" <-- where to place the final message
##          valid locations are: 'cookie', 'body', 'uri', 'parameter'
##      name <-- name of cookie, parameter, or uri
##    transforms <-- array of transforms to do on the message
##      action <-- what transform action to take
##          valid actions are: 'base64', 'base64url', 'prepend', 'append', 'xor', 'netbios', 'netbiosu'
##      value <-- string value to use as a parameter when performing 'action'
[get.client]
headers."User-Agent" = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/105.0.0.0 Safari/537.36"
parameters.MyKey = "value"
[get.client.domain_specific_headers."https://example.com:443"]
User-Agent = "Test"
[get.client.message]
location = "cookie"
name = "sessionID"
[[get.client.transforms]]
action = "base64url"
# SENDS:
# GET /my/uri/path?MyKey=value
# User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/105.0.0.0 Safari/537.36
# Cookie: sessionID=someBase64

################################################
## SERVER
################################################
## Description:
##      server identifies data going from the server back to the agent
## Defaults:
##    headers <-- dictionary of key/value pairs
##      set with server.headers.Key = value
##    parameters <-- dictionary of key/value pairs
##      set with server.parameters.Key = value
##    transforms <-- array of transforms to do on the message
##      action <-- what transform action to take
##         valid actions are: 'base64', 'base64url', 'prepend', 'append', 'xor', 'netbios', 'netbiosu'
##      value <-- string value to use as a parameter when performing 'action'
[get.server.headers]
Server = "Server"
Cache-Control = "max-age=0, no-cache"
[[get.server.transforms]]
action = "xor"
value = "keyHere"
[[get.server.transforms]]
action = "base64url"
[[get.server.transforms]]
action = "prepend"
value = "{\"response\":\""
[[get.server.transforms]]
action = "append"
value = "\"}"
# Sends
# Server: Server
# Cache-Control: max-age=0, no-cache
#
# {"survey_data": "someBase64Here"}

[post]
uris = ["/my/other/path"]
verb = "POST"
[post.client.headers]
"User-Agent" = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/105.0.0.0 Safari/537.36"
[[post.client.transforms]]
action = "xor"
value = "keyHere"
[[post.client.transforms]]
action = "base64url"
# Sends
# POST /my/other/path?query_id=someBase64"
# User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/105.0.0.0 Safari/537.36

[post.server.headers]
Keep-Alive = "true"
[[get.server.transforms]]
action = "netbios"
# Sends
# Keep-Alive: true

# netbios encoded data
```

## Mimic HTTP profile defaults

```toml
# Server and agent have to agree on formats or comms won't work
# ideally pre-load server configurations into profile, at creation time just select saved instance and provide _extra_ info


name = "HTTP-DEFAULT"
################################################
## HTTP GET
################################################
## Description:
##      GET is used to poll teamserver for tasks
## Defaults:
##    verb "GET" <-- "GET" or "POST"
##    uris ["/activity"] <-- string array
[get]
verb = "GET"
uris = ["/index"]

################################################
## CLIENT
################################################
## Description:
##      client identifies data going from the agent to the server
## Defaults:
##    headers <-- dictionary of key/value pairs
##      set with client.headers.Key = value
##    domain_specific_headers <-- dictionary of dictionaries of key/value pairs
##          Allows setting headers specific to callback hosts
##      set with client.domain_specific_headers."domain".key = value
##    parameters <-- dictionary of key/value pairs
##      set with client.parameters.Key = value
##    message <-- dictionary of key/value pairs about the message the agent is sending to the server
##      location "cookie" <-- where to place the final message
##          valid locations are: 'cookie', 'body', 'uri', 'query'
##      name <-- name of cookie, query, or uri
##      transforms <-- array of transforms to do on the message
##          action <-- what transform action to take
##              valid actions are: 'base64', 'base64url', 'prepend', 'append', 'xor', 'netbios', 'netbiosu'
##          value <-- string value to use as a parameter when performing 'action'
[get.client]
headers."User-Agent" = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/105.0.0.0 Safari/537.36"
[get.client.message]
location = "query"
name = "q"
[[get.client.transforms]]
action = "base64url"
# SENDS:
# GET /index?q=value
# User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/105.0.0.0 Safari/537.36

################################################
## SERVER
################################################
## Description:
##      server identifies data going from the server back to the agent
## Defaults:
##    headers <-- dictionary of key/value pairs
##      set with server.headers.Key = value
##    domain_specific_headers <-- dictionary of dictionaries of key/value pairs
##          Allows setting headers specific to callback hosts
##      set with client.domain_specific_headers."domain".key = value
##    parameters <-- dictionary of key/value pairs
##      set with server.parameters.Key = value
##    message <-- dictionary of key/value pairs about the message the server is sending back to the agent
##      The final message will always be in the body of the response
##      transforms <-- array of transforms to do on the message
##          action <-- what transform action to take
##              valid actions are: 'base64', 'base64url', 'prepend', 'append', 'xor', 'netbios', 'netbiosu'
##          value <-- string value to use as a parameter when performing 'action'
[get.server.headers]
Server = "Server"
Cache-Control = "max-age=0, no-cache"

# Sends
# Server: Server
# Cache-Control: max-age=0, no-cache
#

[post]
uris = ["/data"]
verb = "POST"
[post.client.headers]
"User-Agent" = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/105.0.0.0 Safari/537.36"

# Sends
# POST /data"
# User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/105.0.0.0 Safari/537.36

[post.server.headers]
Keep-Alive = "true"

# Sends
# Keep-Alive: true

```
## jquery-c2.4.9.profile
This is an example from https://github.com/threatexpress/malleable-c2/blob/master/jquery-c2.4.9.profile that's made for cobalt strike.
We can pretty easily convert this for `httpx` into TOML as follows:

```toml
# Malleable C2 Profile
# Version: CobaltStrike 4.9
# File: jquery-c2.4.9.profile
# Description: 
#    c2 profile attempting to mimic a jquery.js request
#    uses signed certificates
#    or self-signed certificates
# Authors: @joevest, @andrewchiles, @001SPARTaN, @Charles-Foster-Kane

name = "jQuery CS 4.9 Profile"

[get]
verb = "GET"
uris = ["/jquery-3.3.1.min.js"]

[get.client.headers]
"Keep-Alive" = "timeout=10, max=100"
"Connection" = "Keep-Alive"
User-Agent = "Mozilla/5.0 (Windows NT 6.3; Trident/7.0; rv:11.0) like Gecko"
"Accept" = "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"
"Referer" = "http://code.jquery.com/"
"Accept-Encoding" = "gzip, deflate"

[get.client.message]
location = "cookie"
name = "__cfduid"
[[get.client.transforms]]
action = "base64url"

[get.server.headers]
"Server" = "NetDNA-cache/2.2"
"Cache-Control" = "max-age=0, no-cache"
"Pragma" = "no-cache"
"Connection" = "keep-alive"
"Content-Type" = "application/javascript; charset=utf-8"
[[get.server.transforms]]
action = "xor"
value = "randomKey"
[[get.server.transforms]]
action = "base64url"
[[get.server.transforms]]
action = "prepend"
value = "!function(e,t){\"use strict\";\"object\"==typeof module&&\"object\"==typeof module.exports?module.exports=e.document?t(e,!0):function(e){if(!e.document)throw new Error(\"jQuery requires a window with a document\");return t(e)}:t(e)}(\"undefined\"!=typeof window?window:this,function(e,t){\"use strict\";var n=[],r=e.document,i=Object.getPrototypeOf,o=n.slice,a=n.concat,s=n.push,u=n.indexOf,l={},c=l.toString,f=l.hasOwnProperty,p=f.toString,d=p.call(Object),h={},g=function e(t){return\"function\"==typeof t&&\"number\"!=typeof t.nodeType},y=function e(t){return null!=t&&t===t.window},v={type:!0,src:!0,noModule:!0};function m(e,t,n){var i,o=(t=t||r).createElement(\"script\");if(o.text=e,n)for(i in v)n[i]&&(o[i]=n[i]);t.head.appendChild(o).parentNode.removeChild(o)}function x(e){return null==e?e+\"\":\"object\"==typeof e||\"function\"==typeof e?l[c.call(e)]||\"object\":typeof e}var b=\"3.3.1\",w=function(e,t){return new w.fn.init(e,t)},T=/^[\\s\\uFEFF\\xA0]+|[\\s\\uFEFF\\xA0]+$/g;w.fn=w.prototype={jquery:\"3.3.1\",constructor:w,length:0,toArray:function(){return o.call(this)},get:function(e){return null==e?o.call(this):e<0?this[e+this.length]:this[e]},pushStack:function(e){var t=w.merge(this.constructor(),e);return t.prevObject=this,t},each:function(e){return w.each(this,e)},map:function(e){return this.pushStack(w.map(this,function(t,n){return e.call(t,n,t)}))},slice:function(){return this.pushStack(o.apply(this,arguments))},first:function(){return this.eq(0)},last:function(){return this.eq(-1)},eq:function(e){var t=this.length,n=+e+(e<0?t:0);return this.pushStack(n>=0&&n<t?[this[n]]:[])},end:function(){return this.prevObject||this.constructor()},push:s,sort:n.sort,splice:n.splice},w.extend=w.fn.extend=function(){var e,t,n,r,i,o,a=arguments[0]||{},s=1,u=arguments.length,l=!1;for(\"boolean\"==typeof a&&(l=a,a=arguments[s]||{},s++),\"object\"==typeof a||g(a)||(a={}),s===u&&(a=this,s--);s<u;s++)if(null!=(e=arguments[s]))for(t in e)n=a[t],a!==(r=e[t])&&(l&&r&&(w.isPlainObject(r)||(i=Array.isArray(r)))?(i?(i=!1,o=n&&Array.isArray(n)?n:[]):o=n&&w.isPlainObject(n)?n:{},a[t]=w.extend(l,o,r)):void 0!==r&&(a[t]=r));return a},w.extend({expando:\"jQuery\"+(\"3.3.1\"+Math.random()).replace(/\\D/g,\"\"),isReady:!0,error:function(e){throw new Error(e)},noop:function(){},isPlainObject:function(e){var t,n;return!(!e||\"[object Object]\"!==c.call(e))&&(!(t=i(e))||\"function\"==typeof(n=f.call(t,\"constructor\")&&t.constructor)&&p.call(n)===d)},isEmptyObject:function(e){var t;for(t in e)return!1;return!0},globalEval:function(e){m(e)},each:function(e,t){var n,r=0;if(C(e)){for(n=e.length;r<n;r++)if(!1===t.call(e[r],r,e[r]))break}else for(r in e)if(!1===t.call(e[r],r,e[r]))break;return e},trim:function(e){return null==e?\"\":(e+\"\").replace(T,\"\")},makeArray:function(e,t){var n=t||[];return null!=e&&(C(Object(e))?w.merge(n,\"string\"==typeof e?[e]:e):s.call(n,e)),n},inArray:function(e,t,n){return null==t?-1:u.call(t,e,n)},merge:function(e,t){for(var n=+t.length,r=0,i=e.length;r<n;r++)e[i++]=t[r];return e.length=i,e},grep:function(e,t,n){for(var r,i=[],o=0,a=e.length,s=!n;o<a;o++)(r=!t(e[o],o))!==s&&i.push(e[o]);return i},map:function(e,t,n){var r,i,o=0,s=[];if(C(e))for(r=e.length;o<r;o++)null!=(i=t(e[o],o,n))&&s.push(i);else for(o in e)null!=(i=t(e[o],o,n))&&s.push(i);return a.apply([],s)},guid:1,support:h}),\"function\"==typeof Symbol&&(w.fn[Symbol.iterator]=n[Symbol.iterator]),w.each(\"Boolean Number String Function Array Date RegExp Object Error Symbol\".split(\" \"),function(e,t){l[\"[object \"+t+\"]\"]=t.toLowerCase()});function C(e){var t=!!e&&\"length\"in e&&e.length,n=x(e);return!g(e)&&!y(e)&&(\"array\"===n||0===t||\"number\"==typeof t&&t>0&&t-1 in e)}var E=function(e){var t,n,r,i,o,a,s,u,l,c,f,p,d,h,g,y,v,m,x,b=\"sizzle\"+1*new Date,w=e.document,T=0,C=0,E=ae(),k=ae(),S=ae(),D=function(e,t){return e===t&&(f=!0),0},N={}.hasOwnProperty,A=[],j=A.pop,q=A.push,L=A.push,H=A.slice,O=function(e,t){for(var n=0,r=e.length;n<r;n++)if(e[n]===t)return n;return-1},P=\"\r"
[[get.server.transforms]]
action = "prepend"
value = "/*! jQuery v3.3.1 | (c) JS Foundation and other contributors | jquery.org/license */"
[[get.server.transforms]]
action = "append"
value = "\".(o=t.documentElement,Math.max(t.body[\"scroll\"+e],o[\"scroll\"+e],t.body[\"offset\"+e],o[\"offset\"+e],o[\"client\"+e])):void 0===i?w.css(t,n,s):w.style(t,n,i,s)},t,a?i:void 0,a)}})}),w.each(\"blur focus focusin focusout resize scroll click dblclick mousedown mouseup mousemove mouseover mouseout mouseenter mouseleave change select submit keydown keypress keyup contextmenu\".split(\" \"),function(e,t){w.fn[t]=function(e,n){return arguments.length>0?this.on(t,null,e,n):this.trigger(t)}}),w.fn.extend({hover:function(e,t){return this.mouseenter(e).mouseleave(t||e)}}),w.fn.extend({bind:function(e,t,n){return this.on(e,null,t,n)},unbind:function(e,t){return this.off(e,null,t)},delegate:function(e,t,n,r){return this.on(t,e,n,r)},undelegate:function(e,t,n){return 1===arguments.length?this.off(e,\"**\"):this.off(t,e||\"**\",n)}}),w.proxy=function(e,t){var n,r,i;if(\"string\"==typeof t&&(n=e[t],t=e,e=n),g(e))return r=o.call(arguments,2),i=function(){return e.apply(t||this,r.concat(o.call(arguments)))},i.guid=e.guid=e.guid||w.guid++,i},w.holdReady=function(e){e?w.readyWait++:w.ready(!0)},w.isArray=Array.isArray,w.parseJSON=JSON.parse,w.nodeName=N,w.isFunction=g,w.isWindow=y,w.camelCase=G,w.type=x,w.now=Date.now,w.isNumeric=function(e){var t=w.type(e);return(\"number\"===t||\"string\"===t)&&!isNaN(e-parseFloat(e))},\"function\"==typeof define&&define.amd&&define(\"jquery\",[],function(){return w});var Jt=e.jQuery,Kt=e.$;return w.noConflict=function(t){return e.$===w&&(e.$=Kt),t&&e.jQuery===w&&(e.jQuery=Jt),w},t||(e.jQuery=e.$=w),w});"

[post]
uris = ["/jquery-3.3.2.min.js"]
verb = "POST"

[post.client.headers]
"Accept" = "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"
"Referer" = "http://code.jquery.com/"
"Accept-Encoding" = "gzip, deflate"

[post.client.message]
location = "body"
[[post.client.transforms]]
action = "xor"
value = "someOtherRandomKey"
[[post.client.transforms]]
action = "base64url"

[post.server.headers]
"Server" = "NetDNA-cache/2.2"
"Cache-Control" = "max-age=0, no-cache"
"Pragma" = "no-cache"
"Connection" = "keep-alive"
"Content-Type" = "application/javascript; charset=utf-8"

[[post.server.transforms]]
action = "xor"
value = "yetAnotherSomeRandomKey"
[[post.server.transforms]]
action = "base64url"
[[post.server.transforms]]
action = "prepend"
value = "!function(e,t){\"use strict\";\"object\"==typeof module&&\"object\"==typeof module.exports?module.exports=e.document?t(e,!0):function(e){if(!e.document)throw new Error(\"jQuery requires a window with a document\");return t(e)}:t(e)}(\"undefined\"!=typeof window?window:this,function(e,t){\"use strict\";var n=[],r=e.document,i=Object.getPrototypeOf,o=n.slice,a=n.concat,s=n.push,u=n.indexOf,l={},c=l.toString,f=l.hasOwnProperty,p=f.toString,d=p.call(Object),h={},g=function e(t){return\"function\"==typeof t&&\"number\"!=typeof t.nodeType},y=function e(t){return null!=t&&t===t.window},v={type:!0,src:!0,noModule:!0};function m(e,t,n){var i,o=(t=t||r).createElement(\"script\");if(o.text=e,n)for(i in v)n[i]&&(o[i]=n[i]);t.head.appendChild(o).parentNode.removeChild(o)}function x(e){return null==e?e+\"\":\"object\"==typeof e||\"function\"==typeof e?l[c.call(e)]||\"object\":typeof e}var b=\"3.3.1\",w=function(e,t){return new w.fn.init(e,t)},T=/^[\\s\\uFEFF\\xA0]+|[\\s\\uFEFF\\xA0]+$/g;w.fn=w.prototype={jquery:\"3.3.1\",constructor:w,length:0,toArray:function(){return o.call(this)},get:function(e){return null==e?o.call(this):e<0?this[e+this.length]:this[e]},pushStack:function(e){var t=w.merge(this.constructor(),e);return t.prevObject=this,t},each:function(e){return w.each(this,e)},map:function(e){return this.pushStack(w.map(this,function(t,n){return e.call(t,n,t)}))},slice:function(){return this.pushStack(o.apply(this,arguments))},first:function(){return this.eq(0)},last:function(){return this.eq(-1)},eq:function(e){var t=this.length,n=+e+(e<0?t:0);return this.pushStack(n>=0&&n<t?[this[n]]:[])},end:function(){return this.prevObject||this.constructor()},push:s,sort:n.sort,splice:n.splice},w.extend=w.fn.extend=function(){var e,t,n,r,i,o,a=arguments[0]||{},s=1,u=arguments.length,l=!1;for(\"boolean\"==typeof a&&(l=a,a=arguments[s]||{},s++),\"object\"==typeof a||g(a)||(a={}),s===u&&(a=this,s--);s<u;s++)if(null!=(e=arguments[s]))for(t in e)n=a[t],a!==(r=e[t])&&(l&&r&&(w.isPlainObject(r)||(i=Array.isArray(r)))?(i?(i=!1,o=n&&Array.isArray(n)?n:[]):o=n&&w.isPlainObject(n)?n:{},a[t]=w.extend(l,o,r)):void 0!==r&&(a[t]=r));return a},w.extend({expando:\"jQuery\"+(\"3.3.1\"+Math.random()).replace(/\\D/g,\"\"),isReady:!0,error:function(e){throw new Error(e)},noop:function(){},isPlainObject:function(e){var t,n;return!(!e||\"[object Object]\"!==c.call(e))&&(!(t=i(e))||\"function\"==typeof(n=f.call(t,\"constructor\")&&t.constructor)&&p.call(n)===d)},isEmptyObject:function(e){var t;for(t in e)return!1;return!0},globalEval:function(e){m(e)},each:function(e,t){var n,r=0;if(C(e)){for(n=e.length;r<n;r++)if(!1===t.call(e[r],r,e[r]))break}else for(r in e)if(!1===t.call(e[r],r,e[r]))break;return e},trim:function(e){return null==e?\"\":(e+\"\").replace(T,\"\")},makeArray:function(e,t){var n=t||[];return null!=e&&(C(Object(e))?w.merge(n,\"string\"==typeof e?[e]:e):s.call(n,e)),n},inArray:function(e,t,n){return null==t?-1:u.call(t,e,n)},merge:function(e,t){for(var n=+t.length,r=0,i=e.length;r<n;r++)e[i++]=t[r];return e.length=i,e},grep:function(e,t,n){for(var r,i=[],o=0,a=e.length,s=!n;o<a;o++)(r=!t(e[o],o))!==s&&i.push(e[o]);return i},map:function(e,t,n){var r,i,o=0,s=[];if(C(e))for(r=e.length;o<r;o++)null!=(i=t(e[o],o,n))&&s.push(i);else for(o in e)null!=(i=t(e[o],o,n))&&s.push(i);return a.apply([],s)},guid:1,support:h}),\"function\"==typeof Symbol&&(w.fn[Symbol.iterator]=n[Symbol.iterator]),w.each(\"Boolean Number String Function Array Date RegExp Object Error Symbol\".split(\" \"),function(e,t){l[\"[object \"+t+\"]\"]=t.toLowerCase()});function C(e){var t=!!e&&\"length\"in e&&e.length,n=x(e);return!g(e)&&!y(e)&&(\"array\"===n||0===t||\"number\"==typeof t&&t>0&&t-1 in e)}var E=function(e){var t,n,r,i,o,a,s,u,l,c,f,p,d,h,g,y,v,m,x,b=\"sizzle\"+1*new Date,w=e.document,T=0,C=0,E=ae(),k=ae(),S=ae(),D=function(e,t){return e===t&&(f=!0),0},N={}.hasOwnProperty,A=[],j=A.pop,q=A.push,L=A.push,H=A.slice,O=function(e,t){for(var n=0,r=e.length;n<r;n++)if(e[n]===t)return n;return-1},P=\"\r"
[[post.server.transforms]]
action = "prepend"
value = "/*! jQuery v3.3.1 | (c) JS Foundation and other contributors | jquery.org/license */"
[[post.server.transforms]]
action = "append"
value = "\".(o=t.documentElement,Math.max(t.body[\"scroll\"+e],o[\"scroll\"+e],t.body[\"offset\"+e],o[\"offset\"+e],o[\"client\"+e])):void 0===i?w.css(t,n,s):w.style(t,n,i,s)},t,a?i:void 0,a)}})}),w.each(\"blur focus focusin focusout resize scroll click dblclick mousedown mouseup mousemove mouseover mouseout mouseenter mouseleave change select submit keydown keypress keyup contextmenu\".split(\" \"),function(e,t){w.fn[t]=function(e,n){return arguments.length>0?this.on(t,null,e,n):this.trigger(t)}}),w.fn.extend({hover:function(e,t){return this.mouseenter(e).mouseleave(t||e)}}),w.fn.extend({bind:function(e,t,n){return this.on(e,null,t,n)},unbind:function(e,t){return this.off(e,null,t)},delegate:function(e,t,n,r){return this.on(t,e,n,r)},undelegate:function(e,t,n){return 1===arguments.length?this.off(e,\"**\"):this.off(t,e||\"**\",n)}}),w.proxy=function(e,t){var n,r,i;if(\"string\"==typeof t&&(n=e[t],t=e,e=n),g(e))return r=o.call(arguments,2),i=function(){return e.apply(t||this,r.concat(o.call(arguments)))},i.guid=e.guid=e.guid||w.guid++,i},w.holdReady=function(e){e?w.readyWait++:w.ready(!0)},w.isArray=Array.isArray,w.parseJSON=JSON.parse,w.nodeName=N,w.isFunction=g,w.isWindow=y,w.camelCase=G,w.type=x,w.now=Date.now,w.isNumeric=function(e){var t=w.type(e);return(\"number\"===t||\"string\"===t)&&!isNaN(e-parseFloat(e))},\"function\"==typeof define&&define.amd&&define(\"jquery\",[],function(){return w});var Jt=e.jQuery,Kt=e.$;return w.noConflict=function(t){return e.$===w&&(e.$=Kt),t&&e.jQuery===w&&(e.jQuery=Jt),w},t||(e.jQuery=e.$=w),w});"
```

Similarly, in JSON that would be:

```json
{
  "name": "jQuery CS 4.9 Profile",
  "get": {
    "verb": "GET",
    "uris": ["/jquery-3.3.1.min.js"],
    "client": {
      "headers": {
        "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        "Accept-Encoding": "gzip, deflate",
        "Connection": "Keep-Alive",
        "Keep-Alive": "timeout=10, max=100",
        "Referer": "http://code.jquery.com/",
        "User-Agent": "Mozilla/5.0 (Windows NT 6.3; Trident/7.0; rv:11.0) like Gecko"
      },
      "parameters": null,
      "message": {
        "location": "cookie",
        "name": "__cfduid"
      },
      "transforms": [
        {
          "action": "base64url",
          "value": ""
        }
      ]
    },
    "server": {
      "headers": {
        "Cache-Control": "max-age=0, no-cache",
        "Connection": "keep-alive",
        "Content-Type": "application/javascript; charset=utf-8",
        "Pragma": "no-cache",
        "Server": "NetDNA-cache/2.2"
      },
      "transforms": [
        {
          "action": "xor",
          "value": "randomKey"
        },
        {
          "action": "base64url",
          "value": ""
        },
        {
          "action": "prepend",
          "value": "!function(e,t){\"use strict\";\"object\"==typeof module\u0026\u0026\"object\"==typeof module.exports?module.exports=e.document?t(e,!0):function(e){if(!e.document)throw new Error(\"jQuery requires a window with a document\");return t(e)}:t(e)}(\"undefined\"!=typeof window?window:this,function(e,t){\"use strict\";var n=[],r=e.document,i=Object.getPrototypeOf,o=n.slice,a=n.concat,s=n.push,u=n.indexOf,l={},c=l.toString,f=l.hasOwnProperty,p=f.toString,d=p.call(Object),h={},g=function e(t){return\"function\"==typeof t\u0026\u0026\"number\"!=typeof t.nodeType},y=function e(t){return null!=t\u0026\u0026t===t.window},v={type:!0,src:!0,noModule:!0};function m(e,t,n){var i,o=(t=t||r).createElement(\"script\");if(o.text=e,n)for(i in v)n[i]\u0026\u0026(o[i]=n[i]);t.head.appendChild(o).parentNode.removeChild(o)}function x(e){return null==e?e+\"\":\"object\"==typeof e||\"function\"==typeof e?l[c.call(e)]||\"object\":typeof e}var b=\"3.3.1\",w=function(e,t){return new w.fn.init(e,t)},T=/^[\\s\\uFEFF\\xA0]+|[\\s\\uFEFF\\xA0]+$/g;w.fn=w.prototype={jquery:\"3.3.1\",constructor:w,length:0,toArray:function(){return o.call(this)},get:function(e){return null==e?o.call(this):e\u003c0?this[e+this.length]:this[e]},pushStack:function(e){var t=w.merge(this.constructor(),e);return t.prevObject=this,t},each:function(e){return w.each(this,e)},map:function(e){return this.pushStack(w.map(this,function(t,n){return e.call(t,n,t)}))},slice:function(){return this.pushStack(o.apply(this,arguments))},first:function(){return this.eq(0)},last:function(){return this.eq(-1)},eq:function(e){var t=this.length,n=+e+(e\u003c0?t:0);return this.pushStack(n\u003e=0\u0026\u0026n\u003ct?[this[n]]:[])},end:function(){return this.prevObject||this.constructor()},push:s,sort:n.sort,splice:n.splice},w.extend=w.fn.extend=function(){var e,t,n,r,i,o,a=arguments[0]||{},s=1,u=arguments.length,l=!1;for(\"boolean\"==typeof a\u0026\u0026(l=a,a=arguments[s]||{},s++),\"object\"==typeof a||g(a)||(a={}),s===u\u0026\u0026(a=this,s--);s\u003cu;s++)if(null!=(e=arguments[s]))for(t in e)n=a[t],a!==(r=e[t])\u0026\u0026(l\u0026\u0026r\u0026\u0026(w.isPlainObject(r)||(i=Array.isArray(r)))?(i?(i=!1,o=n\u0026\u0026Array.isArray(n)?n:[]):o=n\u0026\u0026w.isPlainObject(n)?n:{},a[t]=w.extend(l,o,r)):void 0!==r\u0026\u0026(a[t]=r));return a},w.extend({expando:\"jQuery\"+(\"3.3.1\"+Math.random()).replace(/\\D/g,\"\"),isReady:!0,error:function(e){throw new Error(e)},noop:function(){},isPlainObject:function(e){var t,n;return!(!e||\"[object Object]\"!==c.call(e))\u0026\u0026(!(t=i(e))||\"function\"==typeof(n=f.call(t,\"constructor\")\u0026\u0026t.constructor)\u0026\u0026p.call(n)===d)},isEmptyObject:function(e){var t;for(t in e)return!1;return!0},globalEval:function(e){m(e)},each:function(e,t){var n,r=0;if(C(e)){for(n=e.length;r\u003cn;r++)if(!1===t.call(e[r],r,e[r]))break}else for(r in e)if(!1===t.call(e[r],r,e[r]))break;return e},trim:function(e){return null==e?\"\":(e+\"\").replace(T,\"\")},makeArray:function(e,t){var n=t||[];return null!=e\u0026\u0026(C(Object(e))?w.merge(n,\"string\"==typeof e?[e]:e):s.call(n,e)),n},inArray:function(e,t,n){return null==t?-1:u.call(t,e,n)},merge:function(e,t){for(var n=+t.length,r=0,i=e.length;r\u003cn;r++)e[i++]=t[r];return e.length=i,e},grep:function(e,t,n){for(var r,i=[],o=0,a=e.length,s=!n;o\u003ca;o++)(r=!t(e[o],o))!==s\u0026\u0026i.push(e[o]);return i},map:function(e,t,n){var r,i,o=0,s=[];if(C(e))for(r=e.length;o\u003cr;o++)null!=(i=t(e[o],o,n))\u0026\u0026s.push(i);else for(o in e)null!=(i=t(e[o],o,n))\u0026\u0026s.push(i);return a.apply([],s)},guid:1,support:h}),\"function\"==typeof Symbol\u0026\u0026(w.fn[Symbol.iterator]=n[Symbol.iterator]),w.each(\"Boolean Number String Function Array Date RegExp Object Error Symbol\".split(\" \"),function(e,t){l[\"[object \"+t+\"]\"]=t.toLowerCase()});function C(e){var t=!!e\u0026\u0026\"length\"in e\u0026\u0026e.length,n=x(e);return!g(e)\u0026\u0026!y(e)\u0026\u0026(\"array\"===n||0===t||\"number\"==typeof t\u0026\u0026t\u003e0\u0026\u0026t-1 in e)}var E=function(e){var t,n,r,i,o,a,s,u,l,c,f,p,d,h,g,y,v,m,x,b=\"sizzle\"+1*new Date,w=e.document,T=0,C=0,E=ae(),k=ae(),S=ae(),D=function(e,t){return e===t\u0026\u0026(f=!0),0},N={}.hasOwnProperty,A=[],j=A.pop,q=A.push,L=A.push,H=A.slice,O=function(e,t){for(var n=0,r=e.length;n\u003cr;n++)if(e[n]===t)return n;return-1},P=\"\r"
        },
        {
          "action": "prepend",
          "value": "/*! jQuery v3.3.1 | (c) JS Foundation and other contributors | jquery.org/license */"
        },
        {
          "action": "append",
          "value": "\".(o=t.documentElement,Math.max(t.body[\"scroll\"+e],o[\"scroll\"+e],t.body[\"offset\"+e],o[\"offset\"+e],o[\"client\"+e])):void 0===i?w.css(t,n,s):w.style(t,n,i,s)},t,a?i:void 0,a)}})}),w.each(\"blur focus focusin focusout resize scroll click dblclick mousedown mouseup mousemove mouseover mouseout mouseenter mouseleave change select submit keydown keypress keyup contextmenu\".split(\" \"),function(e,t){w.fn[t]=function(e,n){return arguments.length\u003e0?this.on(t,null,e,n):this.trigger(t)}}),w.fn.extend({hover:function(e,t){return this.mouseenter(e).mouseleave(t||e)}}),w.fn.extend({bind:function(e,t,n){return this.on(e,null,t,n)},unbind:function(e,t){return this.off(e,null,t)},delegate:function(e,t,n,r){return this.on(t,e,n,r)},undelegate:function(e,t,n){return 1===arguments.length?this.off(e,\"**\"):this.off(t,e||\"**\",n)}}),w.proxy=function(e,t){var n,r,i;if(\"string\"==typeof t\u0026\u0026(n=e[t],t=e,e=n),g(e))return r=o.call(arguments,2),i=function(){return e.apply(t||this,r.concat(o.call(arguments)))},i.guid=e.guid=e.guid||w.guid++,i},w.holdReady=function(e){e?w.readyWait++:w.ready(!0)},w.isArray=Array.isArray,w.parseJSON=JSON.parse,w.nodeName=N,w.isFunction=g,w.isWindow=y,w.camelCase=G,w.type=x,w.now=Date.now,w.isNumeric=function(e){var t=w.type(e);return(\"number\"===t||\"string\"===t)\u0026\u0026!isNaN(e-parseFloat(e))},\"function\"==typeof define\u0026\u0026define.amd\u0026\u0026define(\"jquery\",[],function(){return w});var Jt=e.jQuery,Kt=e.$;return w.noConflict=function(t){return e.$===w\u0026\u0026(e.$=Kt),t\u0026\u0026e.jQuery===w\u0026\u0026(e.jQuery=Jt),w},t||(e.jQuery=e.$=w),w});"
        }
      ]
    }
  },
  "post": {
    "verb": "POST",
    "uris": ["/jquery-3.3.2.min.js"],
    "client": {
      "headers": {
        "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        "Accept-Encoding": "gzip, deflate",
        "Referer": "http://code.jquery.com/"
      },
      "parameters": null,
      "message": {
        "location": "body",
        "name": ""
      },
      "transforms": [
        {
          "action": "xor",
          "value": "someOtherRandomKey"
        },
        {
          "action": "base64url",
          "value": ""
        }
      ]
    },
    "server": {
      "headers": {
        "Cache-Control": "max-age=0, no-cache",
        "Connection": "keep-alive",
        "Content-Type": "application/javascript; charset=utf-8",
        "Pragma": "no-cache",
        "Server": "NetDNA-cache/2.2"
      },
      "transforms": [
        {
          "action": "xor",
          "value": "yetAnotherSomeRandomKey"
        },
        {
          "action": "base64url",
          "value": ""
        },
        {
          "action": "prepend",
          "value": "!function(e,t){\"use strict\";\"object\"==typeof module\u0026\u0026\"object\"==typeof module.exports?module.exports=e.document?t(e,!0):function(e){if(!e.document)throw new Error(\"jQuery requires a window with a document\");return t(e)}:t(e)}(\"undefined\"!=typeof window?window:this,function(e,t){\"use strict\";var n=[],r=e.document,i=Object.getPrototypeOf,o=n.slice,a=n.concat,s=n.push,u=n.indexOf,l={},c=l.toString,f=l.hasOwnProperty,p=f.toString,d=p.call(Object),h={},g=function e(t){return\"function\"==typeof t\u0026\u0026\"number\"!=typeof t.nodeType},y=function e(t){return null!=t\u0026\u0026t===t.window},v={type:!0,src:!0,noModule:!0};function m(e,t,n){var i,o=(t=t||r).createElement(\"script\");if(o.text=e,n)for(i in v)n[i]\u0026\u0026(o[i]=n[i]);t.head.appendChild(o).parentNode.removeChild(o)}function x(e){return null==e?e+\"\":\"object\"==typeof e||\"function\"==typeof e?l[c.call(e)]||\"object\":typeof e}var b=\"3.3.1\",w=function(e,t){return new w.fn.init(e,t)},T=/^[\\s\\uFEFF\\xA0]+|[\\s\\uFEFF\\xA0]+$/g;w.fn=w.prototype={jquery:\"3.3.1\",constructor:w,length:0,toArray:function(){return o.call(this)},get:function(e){return null==e?o.call(this):e\u003c0?this[e+this.length]:this[e]},pushStack:function(e){var t=w.merge(this.constructor(),e);return t.prevObject=this,t},each:function(e){return w.each(this,e)},map:function(e){return this.pushStack(w.map(this,function(t,n){return e.call(t,n,t)}))},slice:function(){return this.pushStack(o.apply(this,arguments))},first:function(){return this.eq(0)},last:function(){return this.eq(-1)},eq:function(e){var t=this.length,n=+e+(e\u003c0?t:0);return this.pushStack(n\u003e=0\u0026\u0026n\u003ct?[this[n]]:[])},end:function(){return this.prevObject||this.constructor()},push:s,sort:n.sort,splice:n.splice},w.extend=w.fn.extend=function(){var e,t,n,r,i,o,a=arguments[0]||{},s=1,u=arguments.length,l=!1;for(\"boolean\"==typeof a\u0026\u0026(l=a,a=arguments[s]||{},s++),\"object\"==typeof a||g(a)||(a={}),s===u\u0026\u0026(a=this,s--);s\u003cu;s++)if(null!=(e=arguments[s]))for(t in e)n=a[t],a!==(r=e[t])\u0026\u0026(l\u0026\u0026r\u0026\u0026(w.isPlainObject(r)||(i=Array.isArray(r)))?(i?(i=!1,o=n\u0026\u0026Array.isArray(n)?n:[]):o=n\u0026\u0026w.isPlainObject(n)?n:{},a[t]=w.extend(l,o,r)):void 0!==r\u0026\u0026(a[t]=r));return a},w.extend({expando:\"jQuery\"+(\"3.3.1\"+Math.random()).replace(/\\D/g,\"\"),isReady:!0,error:function(e){throw new Error(e)},noop:function(){},isPlainObject:function(e){var t,n;return!(!e||\"[object Object]\"!==c.call(e))\u0026\u0026(!(t=i(e))||\"function\"==typeof(n=f.call(t,\"constructor\")\u0026\u0026t.constructor)\u0026\u0026p.call(n)===d)},isEmptyObject:function(e){var t;for(t in e)return!1;return!0},globalEval:function(e){m(e)},each:function(e,t){var n,r=0;if(C(e)){for(n=e.length;r\u003cn;r++)if(!1===t.call(e[r],r,e[r]))break}else for(r in e)if(!1===t.call(e[r],r,e[r]))break;return e},trim:function(e){return null==e?\"\":(e+\"\").replace(T,\"\")},makeArray:function(e,t){var n=t||[];return null!=e\u0026\u0026(C(Object(e))?w.merge(n,\"string\"==typeof e?[e]:e):s.call(n,e)),n},inArray:function(e,t,n){return null==t?-1:u.call(t,e,n)},merge:function(e,t){for(var n=+t.length,r=0,i=e.length;r\u003cn;r++)e[i++]=t[r];return e.length=i,e},grep:function(e,t,n){for(var r,i=[],o=0,a=e.length,s=!n;o\u003ca;o++)(r=!t(e[o],o))!==s\u0026\u0026i.push(e[o]);return i},map:function(e,t,n){var r,i,o=0,s=[];if(C(e))for(r=e.length;o\u003cr;o++)null!=(i=t(e[o],o,n))\u0026\u0026s.push(i);else for(o in e)null!=(i=t(e[o],o,n))\u0026\u0026s.push(i);return a.apply([],s)},guid:1,support:h}),\"function\"==typeof Symbol\u0026\u0026(w.fn[Symbol.iterator]=n[Symbol.iterator]),w.each(\"Boolean Number String Function Array Date RegExp Object Error Symbol\".split(\" \"),function(e,t){l[\"[object \"+t+\"]\"]=t.toLowerCase()});function C(e){var t=!!e\u0026\u0026\"length\"in e\u0026\u0026e.length,n=x(e);return!g(e)\u0026\u0026!y(e)\u0026\u0026(\"array\"===n||0===t||\"number\"==typeof t\u0026\u0026t\u003e0\u0026\u0026t-1 in e)}var E=function(e){var t,n,r,i,o,a,s,u,l,c,f,p,d,h,g,y,v,m,x,b=\"sizzle\"+1*new Date,w=e.document,T=0,C=0,E=ae(),k=ae(),S=ae(),D=function(e,t){return e===t\u0026\u0026(f=!0),0},N={}.hasOwnProperty,A=[],j=A.pop,q=A.push,L=A.push,H=A.slice,O=function(e,t){for(var n=0,r=e.length;n\u003cr;n++)if(e[n]===t)return n;return-1},P=\"\r"
        },
        {
          "action": "prepend",
          "value": "/*! jQuery v3.3.1 | (c) JS Foundation and other contributors | jquery.org/license */"
        },
        {
          "action": "append",
          "value": "\".(o=t.documentElement,Math.max(t.body[\"scroll\"+e],o[\"scroll\"+e],t.body[\"offset\"+e],o[\"offset\"+e],o[\"client\"+e])):void 0===i?w.css(t,n,s):w.style(t,n,i,s)},t,a?i:void 0,a)}})}),w.each(\"blur focus focusin focusout resize scroll click dblclick mousedown mouseup mousemove mouseover mouseout mouseenter mouseleave change select submit keydown keypress keyup contextmenu\".split(\" \"),function(e,t){w.fn[t]=function(e,n){return arguments.length\u003e0?this.on(t,null,e,n):this.trigger(t)}}),w.fn.extend({hover:function(e,t){return this.mouseenter(e).mouseleave(t||e)}}),w.fn.extend({bind:function(e,t,n){return this.on(e,null,t,n)},unbind:function(e,t){return this.off(e,null,t)},delegate:function(e,t,n,r){return this.on(t,e,n,r)},undelegate:function(e,t,n){return 1===arguments.length?this.off(e,\"**\"):this.off(t,e||\"**\",n)}}),w.proxy=function(e,t){var n,r,i;if(\"string\"==typeof t\u0026\u0026(n=e[t],t=e,e=n),g(e))return r=o.call(arguments,2),i=function(){return e.apply(t||this,r.concat(o.call(arguments)))},i.guid=e.guid=e.guid||w.guid++,i},w.holdReady=function(e){e?w.readyWait++:w.ready(!0)},w.isArray=Array.isArray,w.parseJSON=JSON.parse,w.nodeName=N,w.isFunction=g,w.isWindow=y,w.camelCase=G,w.type=x,w.now=Date.now,w.isNumeric=function(e){var t=w.type(e);return(\"number\"===t||\"string\"===t)\u0026\u0026!isNaN(e-parseFloat(e))},\"function\"==typeof define\u0026\u0026define.amd\u0026\u0026define(\"jquery\",[],function(){return w});var Jt=e.jQuery,Kt=e.$;return w.noConflict=function(t){return e.$===w\u0026\u0026(e.$=Kt),t\u0026\u0026e.jQuery===w\u0026\u0026(e.jQuery=Jt),w},t||(e.jQuery=e.$=w),w});"
        }
      ]
    }
  }
}
```