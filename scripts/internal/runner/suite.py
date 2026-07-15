"""Runner-injected client surface for ikigenba suite services."""

import hashlib
import http.client
import json
import os
import shutil
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


class _Files:
    """Namespace reserved for the file-share client surface."""


files = _Files()
