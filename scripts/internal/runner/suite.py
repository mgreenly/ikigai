"""Runner-injected client surface for ikigenba suite services."""

import hashlib
import http.client
import json
import os
import tempfile
import urllib.error
import urllib.parse
import urllib.request


HTTP_TIMEOUT_SECONDS = 30
_EVENT_UNSET = object()
_event_value = _EVENT_UNSET


class ToolError(Exception):
    """A failure returned by a suite service."""

    def __init__(self, code, message):
        super().__init__(f"{code}: {message}")
        self.code = code
        self.message = message


def _runtime_value(name):
    try:
        return os.environ[name]
    except KeyError:
        raise RuntimeError(
            f"suite: not running under the scripts runner (missing {name})"
        ) from None


def event():
    """Return the trigger payload verbatim, parsing it only once."""

    global _event_value
    if _event_value is _EVENT_UNSET:
        _event_value = json.loads(_runtime_value("EVENT_JSON"))
    return _event_value


def mcp(service, verb, arguments=None):
    """Call a suite service's MCP tool; return its structured result or prose."""

    services = json.loads(_runtime_value("SUITE_SERVICES"))
    if service not in services:
        raise ValueError(f"unknown suite service: {service}")

    payload = {
        "jsonrpc": "2.0",
        "id": 1,
        "method": "tools/call",
        "params": {"name": verb, "arguments": {} if arguments is None else arguments},
    }
    request = urllib.request.Request(
        services[service].rstrip("/") + "/mcp",
        data=json.dumps(payload).encode("utf-8"),
        headers={
            "X-Owner-Email": _runtime_value("SUITE_OWNER_EMAIL"),
            "X-Client-Id": f"scripts:{_runtime_value('SUITE_SCRIPT_ID')}",
            "Content-Type": "application/json",
        },
        method="POST",
    )

    try:
        with urllib.request.urlopen(request, timeout=HTTP_TIMEOUT_SECONDS) as response:
            status = response.status
            body = response.read()
    except urllib.error.HTTPError as exc:
        raise ToolError("internal", f"{service} /mcp returned {exc.code}") from None
    except (OSError, TimeoutError, urllib.error.URLError, http.client.IncompleteRead) as exc:
        raise ToolError("source_unavailable", str(exc)) from None

    if status != 200:
        raise ToolError("internal", f"{service} /mcp returned {status}")
    try:
        message = json.loads(body)
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ToolError("source_unavailable", str(exc)) from None

    if "error" in message:
        error = message["error"]
        code = "validation" if error.get("code") == -32602 else "internal"
        raise ToolError(code, error.get("message", ""))

    result = message["result"]
    if result.get("isError"):
        structured = result.get("structuredContent")
        if structured is not None:
            raise ToolError(structured["code"], structured["message"])
        raise ToolError("internal", _content_text(result.get("content", [])))
    if "structuredContent" in result:
        return result["structuredContent"]
    return _content_text(result.get("content", []))


def _content_text(content):
    return "".join(block.get("text", "") for block in content)


def _tool_error_for_status(status, body=b"", map_validation=False):
    if status == 400 and map_validation:
        return ToolError("validation", body.decode("utf-8", errors="replace"))
    if status == 404:
        return ToolError("not_found", "source returned 404")
    if status == 409:
        return ToolError("conflict", "source returned 409")
    return ToolError("source_unavailable", f"source returned {status}")


def _open_http(method, url, headers=None, body_file=None):
    parsed = urllib.parse.urlsplit(url)
    connection = http.client.HTTPConnection(
        parsed.hostname, parsed.port, timeout=HTTP_TIMEOUT_SECONDS
    )
    target = urllib.parse.urlunsplit(("", "", parsed.path or "/", parsed.query, ""))
    request_headers = {} if headers is None else dict(headers)
    try:
        if body_file is None:
            connection.request(method, target, headers=request_headers)
        else:
            connection.putrequest(method, target)
            for name, value in request_headers.items():
                connection.putheader(name, value)
            connection.endheaders()
    except (OSError, TimeoutError, http.client.HTTPException) as exc:
        connection.close()
        raise ToolError("source_unavailable", str(exc)) from None
    if body_file is not None:
        while True:
            try:
                chunk = body_file.read(64 * 1024)
            except OSError:
                connection.close()
                raise
            if not chunk:
                break
            try:
                connection.send(chunk)
            except (OSError, TimeoutError, http.client.HTTPException) as exc:
                connection.close()
                raise ToolError("source_unavailable", str(exc)) from None
    try:
        return connection, connection.getresponse()
    except (OSError, TimeoutError, http.client.HTTPException) as exc:
        connection.close()
        raise ToolError("source_unavailable", str(exc)) from None


def _read_response(response):
    try:
        return response.read()
    except (OSError, TimeoutError, http.client.HTTPException) as exc:
        raise ToolError("source_unavailable", str(exc)) from None


def _remove_destination(dest):
    try:
        os.unlink(dest)
    except FileNotFoundError:
        pass


def _stream_response(response, dest):
    directory = os.path.dirname(os.path.abspath(dest))
    temporary = tempfile.NamedTemporaryFile(dir=directory, delete=False)
    temporary_path = temporary.name
    digest = hashlib.sha256()
    size = 0
    try:
        with temporary:
            while True:
                try:
                    chunk = response.read(64 * 1024)
                except (OSError, TimeoutError, http.client.HTTPException) as exc:
                    raise ToolError("source_unavailable", str(exc)) from None
                if not chunk:
                    if response.length not in (None, 0):
                        raise ToolError(
                            "source_unavailable",
                            f"response ended with {response.length} bytes remaining",
                        )
                    break
                temporary.write(chunk)
                digest.update(chunk)
                size += len(chunk)
        os.replace(temporary_path, dest)
    except Exception:
        try:
            os.unlink(temporary_path)
        except FileNotFoundError:
            pass
        raise
    return {"path": dest, "size": size, "content_hash": digest.hexdigest()}


def _allowed_content_ports():
    origins = list(json.loads(_runtime_value("SUITE_SERVICES")).values())
    origins.append(_runtime_value("SUITE_FILES_BASE_URL"))
    ports = set()
    for origin in origins:
        try:
            parsed = urllib.parse.urlsplit(origin)
            if parsed.port is not None:
                ports.add(parsed.port)
        except ValueError:
            continue
    return ports


def fetch(content_url, dest):
    """Fetch a suite content URL to a local file."""

    try:
        parsed = urllib.parse.urlsplit(content_url)
        port = parsed.port
    except (TypeError, ValueError) as exc:
        raise ToolError("validation", f"content URL must have a valid port: {exc}") from None
    if parsed.scheme != "http":
        raise ToolError("validation", "content URL scheme must be http")
    if parsed.hostname not in {"127.0.0.1", "::1"}:
        raise ToolError("validation", "content URL host must be 127.0.0.1 or ::1")
    if port is None:
        raise ToolError("validation", "content URL must have an explicit port")
    if port not in _allowed_content_ports():
        raise ToolError("validation", "content URL port must be a suite service port")

    try:
        connection, response = _open_http("GET", content_url)
        try:
            if response.status < 200 or response.status >= 300:
                body = _read_response(response)
                _remove_destination(dest)
                raise _tool_error_for_status(response.status, body)
            return _stream_response(response, dest)
        finally:
            response.close()
            connection.close()
    except ToolError:
        _remove_destination(dest)
        raise


def _share_path(p):
    """Return the share path rooted: prefix '/' when missing."""

    if p and not p.startswith("/"):
        return "/" + p
    return p


class _Files:
    """Filesystem client; share paths are absolute, with relative spellings rooted."""

    @staticmethod
    def _url(route, params=None):
        base = _runtime_value("SUITE_FILES_BASE_URL").rstrip("/")
        query = urllib.parse.urlencode(params or {})
        return base + route + (("?" + query) if query else "")

    @staticmethod
    def _headers():
        return {"X-Client-Id": f"scripts:{_runtime_value('SUITE_SCRIPT_ID')}"}

    def _json(self, method, route, params=None):
        connection, response = _open_http(
            method, self._url(route, params), headers=self._headers()
        )
        try:
            body = _read_response(response)
            if response.status < 200 or response.status >= 300:
                raise _tool_error_for_status(response.status, body, map_validation=True)
            try:
                return json.loads(body)
            except (UnicodeDecodeError, json.JSONDecodeError) as exc:
                raise ToolError("source_unavailable", str(exc)) from None
        finally:
            response.close()
            connection.close()

    def _empty(self, method, route, params):
        connection, response = _open_http(
            method, self._url(route, params), headers=self._headers()
        )
        try:
            body = _read_response(response)
            if response.status < 200 or response.status >= 300:
                raise _tool_error_for_status(response.status, body, map_validation=True)
            return None
        finally:
            response.close()
            connection.close()

    def list(self, path=None, cursor=None, limit=None):
        """List entries; a given share path is absolute or treated as rooted."""

        params = {}
        if path is not None:
            params["path"] = _share_path(path)
        if cursor is not None:
            params["cursor"] = cursor
        if limit is not None:
            params["limit"] = limit
        return self._json("GET", "/list", params)

    def stat(self, path):
        """Return metadata; the share path is absolute or treated as rooted."""

        return self._json("GET", "/stat", {"path": _share_path(path)})

    def get(self, share_path, dest):
        """Download an absolute (or rooted relative) share path to local dest."""

        try:
            connection, response = _open_http(
                "GET",
                self._url("/content", {"path": _share_path(share_path)}),
                headers=self._headers(),
            )
            try:
                if response.status < 200 or response.status >= 300:
                    body = _read_response(response)
                    _remove_destination(dest)
                    raise _tool_error_for_status(
                        response.status, body, map_validation=True
                    )
                return _stream_response(response, dest)
            finally:
                response.close()
                connection.close()
        except ToolError:
            _remove_destination(dest)
            raise

    def put(self, source, share_path):
        """Upload local source to an absolute (or rooted relative) share path."""

        with open(source, "rb") as body:
            headers = self._headers()
            headers["Content-Length"] = str(os.fstat(body.fileno()).st_size)
            connection, response = _open_http(
                "PUT",
                self._url("/content", {"path": _share_path(share_path)}),
                headers=headers,
                body_file=body,
            )
            try:
                response_body = _read_response(response)
                if response.status < 200 or response.status >= 300:
                    raise _tool_error_for_status(
                        response.status, response_body, map_validation=True
                    )
                try:
                    return json.loads(response_body)
                except (UnicodeDecodeError, json.JSONDecodeError) as exc:
                    raise ToolError("source_unavailable", str(exc)) from None
            finally:
                response.close()
                connection.close()

    def delete(self, path):
        """Delete an absolute (or rooted relative) share path if it exists."""

        return self._empty("DELETE", "/content", {"path": _share_path(path)})

    def move(self, src, dest):
        """Move between absolute (or rooted relative) share paths."""

        return self._empty(
            "POST", "/move", {"from": _share_path(src), "to": _share_path(dest)}
        )

    def mkdir(self, path):
        """Create an absolute (or rooted relative) share directory."""

        return self._empty("POST", "/mkdir", {"path": _share_path(path)})


files = _Files()
