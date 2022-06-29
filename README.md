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

Before running, some dynamic linked libraries are required. It can be compiled from `common` directory.
Another option is modifying `common/makefile` and link them statically.

## Data format definition

TCP socket between server and client can receive messages from both side and send to the other side.
To simplify communication, it uses require-response model.
When a client starts a new request, the server will send only one response, yet a socket can be reused.
This means the message is context-free, as the response will only depend on the request and current server status.

| Functionality | Request | Response |
|---|---|---|
| Login | "login"(16) username(64) passwd(64) | "login"(16) session-ID(128) |
| Register | "register"(16) username(64) passwd(64) invitation-code(8) | "register"(16) "success"(128) |
| Download | "download"(16) session-ID(128) pathlen(16) path(pathlen) | "download"(16) sha512(128) filesz(16) |
| Data | "downdata"(16) sha512(128) offset(16) length(16) | "downdata"(16) data(length) |
| Upload | "upload"(16) session-ID(128) sha512(128) filesz(16) pathlen(16) path(pathlen) | "upload"(16) "pending/done"(16) |
| Ready | "upready"(16) sha512(128) | "upready" offset(16) length(16) |
| Data | "updata"(16) sha512(128) offset(16) length(16) data(length) | "updata"(16) "pending/done"(16) |
| Delete | "delete"(16) session-ID(128) pathlen(16) path(pathlen) | "delete"(16) "success"(16) |

The data except raw file data are all in string form so that the byte order does not need to specify.

## Client usage

To communicate with server using data format defined above, a client should firstly support TCP socket communication and connect to the address and port of the server.

### Register and login

With the login request, a client should specify username and password in request, and they both have 64-bit width. The password should be encrypted with SHA256 so that it will fit the length in this request.

Register is almost the same as login, with additional invitation code stored in database.

### Download files

To download a file, firstly a client should send a `download` request and extract SHA512 hash string and file size from response. After that, the client is supposed to use this hash string and send `downdata` to get file segments with offset and data length specified. This is to limit the maximum length of response message.

Also, if path in `download` request is a directory, the data downloaded will be a newly generated text file, which contains the structure of this directory, with each line as filename, file type (`r`: regular or `d`: directory), and file size in bytes.

### Upload files

Due to the client-server model, to send file data to server, clients need to start a request with `upready`, after `upload` requests have determined the file information and created an empty file. Then the server will look into this file and calculates file offset and length assigned to a specific client. With this, multiple clients uploading a same file will be easily implemented.

### Delete files

Request data of deleting files and directories is the same as `download`, which needs the path length and path. It will always response with `success`, as it calls a bash command like `rm -rf /foo/bar`.

## Error handling

Sometimes the command in request is invalid, and then the server will send a message indicating some error information. The basic format is `message(16) 'some message'(128)`. Messages are listed below.

- User does not exist.
- Incorrect password.
- Invalid invitation code.
- User already exists.
- File not found.
- Session not found.
- Unrecognizable data format.
- Request exceeds size limits.
- Unrequested upload data.
- Unable to create file.
- Wrong hash value.
