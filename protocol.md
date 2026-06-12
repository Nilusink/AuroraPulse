# Aurora Device Communications Protocl
## Default Transport
- HTTP/TCP
- Port `12345`
- UTF-8 encoded JSON

## Connection States
```mermaid
stateDiagram-v2
    [*] --> Disconnected
    Disconnected --> NetworkConnected: OK
    Disconnected --> Error: network error

    NetworkConnected --> Error: Timeout
    NetworkConnected --> Error: Connection Error
    NetworkConnected --> Unauthorized: connection OK

    Error --> Disconnected: Retry

    Unauthorized --> Registered: register

    Registered --> Idle: Invalid secret
    Registered --> Error: timeout
    Registered --> KeepAlive: OK

    KeepAlive --> KeepAlive: every 5s
    KeepAlive --> Error: timeout
    KeepAlive --> Unauthorized: invalid token
    KeepAlive --> Unauthorized: exipred token
```


# Network Connection
```mermaid
sequenceDiagram
    participant Device@{"type": "actor"}
    participant Server@{"type": "control"}

    Device->>Server: GET static IP
    Server-->>Device: assigned IP
```

## Syntax
**Request**
```
GET http://<manager ip>:<port>/device/<device id>/address
```

<br>

**SUCCESS**

Code: `200 OK` \
Content-Type: `application/json`

```json
{
    "ip": "<device ip>",
    "port": <device port>
}
```

<br>

**ERROR**

Code: `404 Not Found` \
Content-Type: `application/json`

```json
{
    "error": "invalid ID"
}
```

<br><br>

# Register
```mermaid
sequenceDiagram
    participant Device@{"type": "actor"}
    participant Server@{"type": "control"}
    participant DB@{"type": "database"} as Device DB

    Device->>Server: Register
    Server->>DB: get device data
    DB-->>Server: device data OR None
    Server-->>Device: IF None: GET endpoints
    Device-->>Server: List of endpoints
    Server-->>DB: IF None: register device
    Server->>Device: access token / invalid secret / device already registered
```

## Syntax
### Register Request
```
POST http://<manager ip>:<port>/device/<device id>/register
```

<br>

Content-Type: `application/json`

```json
{
    "sig": "<hmac>",
    "ts": 10293480
}
```

<br>

**SUCCESS**

Code: `200 OK` \
Content-Type: `application/json`

```json
{
    "success": true
    "token": "<128 bit token>",
    "expires": 12345
}
```

<br>

**ERROR**

Code: `401 Unauthorized` \
Content-Type: `application/json`

```json
{
    "success": false,
    "error": "invalid secret"
}
```

Code: `409 Conflict` \
Content-Type: `application/json`

```json
{
    "success": false,
    "error": "device already registered"
}
```

<br>

### GET Endpoints
```
GET http://<client-ip>:<port>/endpoints
```

<br>

**Expected result**

Code: `200 OK` \
Content-Type: `application/json`

```json
[
    "endpoint1",
    "endpoint2",
    ...
]
```

<br><br>

# Keep alive
```mermaid
sequenceDiagram
    participant Device@{"type": "actor"}
    participant Server@{"type": "control"}

    Device->>Server: KeepAlive
    Server-->>Device: OK / invalid token
```

## Syntax
### Request
```
POST http://<manager ip>:<port>/device/<device id>/keepalive
```

Content-Type: `application/json`

```json
{
    "token": "<128 bit token>"
}
```

<br>

**SUCCESS**

Code: `200 OK` \
Content-Type: `application/json`

```json
{
    "succes": true
}
```

<br>

**ERROR**

Code: `401 Unauthorized` \
Content-Type: `application/json`

```json
{
    "success": false,
    "error": "invalid or expired token"
}
```