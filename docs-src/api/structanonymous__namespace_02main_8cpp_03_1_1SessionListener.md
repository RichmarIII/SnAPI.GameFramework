# anonymous_namespace{main.cpp}::SessionListener

Session listener that logs high-signal connection lifecycle events.

## Public Members

<div class="snapi-api-card" markdown="1">
### `std::string anonymous_namespace{main.cpp}::SessionListener::Label`
</div>
<div class="snapi-api-card" markdown="1">
### `NetSession* anonymous_namespace{main.cpp}::SessionListener::SessionRef`
</div>

## Public Functions

<div class="snapi-api-card" markdown="1">
### `anonymous_namespace{main.cpp}::SessionListener::SessionListener(std::string LabelValue, NetSession *SessionValue=nullptr)`

**Parameters**

- `LabelValue`: 
- `SessionValue`:
</div>
<div class="snapi-api-card" markdown="1">
### `void anonymous_namespace{main.cpp}::SessionListener::OnConnectionAdded(const NetConnectionEvent &Event) override`

**Parameters**

- `Event`:
</div>
<div class="snapi-api-card" markdown="1">
### `void anonymous_namespace{main.cpp}::SessionListener::OnConnectionReady(const NetConnectionEvent &Event) override`

**Parameters**

- `Event`:
</div>
<div class="snapi-api-card" markdown="1">
### `void anonymous_namespace{main.cpp}::SessionListener::OnConnectionClosed(const NetConnectionEvent &Event) override`

**Parameters**

- `Event`:
</div>
<div class="snapi-api-card" markdown="1">
### `void anonymous_namespace{main.cpp}::SessionListener::OnConnectionMigrated(const NetConnectionEvent &Event, const NetEndpoint &PreviousRemote) override`

**Parameters**

- `Event`: 
- `PreviousRemote`:
</div>
