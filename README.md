# Cloud drive app backend

This repository contains a simple backend of cloud drive based on TCP socket.
The environment is Linux and GNU C++, also requires an implemention of MySQL database.

## Server startup

`server` is an executable file that can listen on a specific port, receive request and then make response.
The arguments are listed below:
- `--port PORT`: Listen on port PORT(0 ~ 65535)
- `rootpath ROOTPATH`: Store files in ROOTPATH
- `--daemon`: [Option] Choose to run as daemon.
- `logname LOGPATH`: [Option] Record log in LOGPATH.
- `--help`: [Option] Show instructions.

## Data format definition

TCP socket between server and client can receive messages from both side and send to the other side.
To simplify communication, it uses require-response model.
When a client starts a new request, the server will send only one response, yet a socket can be reused.
This means the message is context-free, as the response will only depend on the request and current server status.

| Functionality | Request | Response |
|---|---|---|
| Login | "login"(16) username(64) passwd(64) | "login"(16) session-ID(128) |
| Register | "register"(16) username(64) passwd(64) invitation-code(8) | "register"(16) "success"(128) |
| Download | "download"(16) session-ID(128) path-len(16) path(pathlen) | "download"(16) sha512(128) filesz(16) |
| Data | "downdata"(16) sha512(128) offset(16) length(16) | "downdata"(16) data(length) |
| Upload | "upload"(16) session-ID(128) sha512(128) filesz(16) path-len(16) path(pathlen) | "upload"(16) "pending"(16) |
| Ready | "upready"(16) sha512(128) | "upready" offset(16) length(16) |
| Data | "updata"(16) sha512(128) offset(16) length(16) data(length) | "updata"(16) "pending/done"(16) |

The data except raw file data are all in string form so that the byte order does not need to specify.
