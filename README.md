# fshare 2.0 - Simple File Sharing System

## How to build and use

### fshared (server)

```
$ ./fshared -p <port_num> -d <dir_name>
```
- dir_name : The name of the directory you want to share.

### fshare (client)

```
$ ./fshare <ip_addr>:<port_num>
```
- Creates a client that automatically synchronizes with a shared directory on the server.

## Description

- 클라이언트의 디렉토리는 모두 비어있다고 가정한다.
- 서버와 클라이언트 모두 파일 이름과 버전에 대한 메타데이터를 담은 linked list를 가지고 있다.

---

- 클라이언트쪽 디렉토리에 새로운 파일이 생길 경우 자동으로 서버에 `put`(업로드) 한다. (using `inotify()`)
- 서버는 파일을 받아 버전 정보를 업데이트한 뒤, 해당 클라이언트에게 버전 넘버를 알려준다.
- 클라이언트는 서버로부터 받은 버전 넘버를 저장한다.

---

- 클라이언트가 지속적으로 서버에 `list`를 요청한다.
- 서버는 파일의 이름과 버전 정보를 함께 넘겨준다. 
- 클라이언트가 가진 파일의 버전과 서버의 버전이 다를 경우 서버로부터 해당 file과 버전 정보를 `get`(다운로드) 한다. (파일이 없는 경우도 해당된다.)


## More

-  처음 서버를 실행시켰을 때, 서버가 관리하는 디렉토리에 이미 존재하는 파일들이 있었을 경우에 대한 해결 방법이 필요하다.
- 서버나 클라이언트 프로그램이 도중에 꺼졌을 경우와 위와 같은 경우를 대비해 hidden file에 파일 목록과 버전 정보를 저장해 두어야 한다.
- 하나의 client가 어떤 파일을 open했을 때, 즉 수정하고 있을 때에는 다른 client들이 다운로드를 하지 못하도록 막을 필요가 있다.
- **BUG** : 클라이언트가 서버로부터 다른 클라이언트에서 생성된 파일을 받아왔을 때, 클라이언트 입장에서 `inotify`가 새로운 파일으로 인식하여 서버에 업로드하기 때문에 동일한 파일을 두번 다운로드 받는 클라이언트가 생긴다.