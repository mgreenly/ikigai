# JSON Schema Validation Research

## Question

Does yyjson support JSON Schema validation? What are our options for validating that a tool's `--schema` output is valid JSON Schema?

## yyjson Capabilities

[yyjson](https://github.com/ibireme/yyjson) is a high-performance JSON parser. It does **not** provide JSON Schema validation.

**What yyjson does:**
- Parse JSON (RFC 8259 compliant)
- JSON Pointer, JSON Patch, JSON Merge Patch
- Build and serialize JSON documents
- Custom memory allocator support

**What yyjson does NOT do:**
- JSON Schema validation
- Schema-based data validation

## Available C Libraries for JSON Schema

### Pure C Libraries

1. **[jsonschema-c](https://github.com/helmut-jacob/jsonschema-c)**
   - Based on json-c library
   - Supports JSON Schema Draft v4
   - Two components: schema validator and instance validator
   - Dependency: json-c (we use yyjson)

2. **[jsonc-daccord](https://github.com/domoslabs/jsonc-daccord)**
   - Lightweight JSON Schema validator
   - Based on json-c library
   - Popular in OpenWRT communities
   - Dependency: json-c (we use yyjson)

### C++ Libraries

3. **[RapidJSON](https://rapidjson.org/md_doc_schema.html)**
   - JSON Schema Draft v4
   - SAX-based streaming validation
   - Would require C++ compilation

4. **[valijson](https://github.com/tristanpenman/valijson)**
   - Header-only C++
   - Multiple parser backends (could potentially use yyjson)
   - Would require C++ compilation

## What We Actually Need

**Critical insight:** We don't need to validate data against a schema. We only need to validate that the schema itself is structurally valid.

### Validation Requirements

When a tool returns its `--schema`:
1. The output must be valid JSON (yyjson handles this)
2. The schema must have required top-level fields (`name`, `description`, `parameters`)
3. The `parameters` object must be a valid JSON Schema (Draft 2020-12 or compatible)

### What "Valid JSON Schema" Means for Us

The schema must be parseable and translatable to all three providers. Key structural requirements:

```
parameters:
  type: "object"           (required)
  properties: {...}        (required)
  required: [...]          (optional, array of strings)
```

Each property in `properties` must have:
- `type`: one of string, integer, number, boolean, array, object
- `description`: string (recommended)

Optional property fields:
- `enum`: array of allowed values
- `items`: for arrays, schema of array elements
- `minimum`, `maximum`: for numbers
- Nested `properties` for object types

## Recommendation: Manual Validation

**Don't add a JSON Schema validation library.**

Rationale:
1. Available C libraries depend on json-c (not yyjson)
2. We only need structural validation, not full JSON Schema compliance
3. Manual validation is straightforward (~100-200 lines)
4. Avoids new dependency and build complexity

### Validation We Should Implement

```c
// Pseudo-code for schema validation
bool validate_tool_schema(yyjson_val *schema) {
    // 1. Required top-level fields
    if (!has_string(schema, "name")) return false;
    if (!has_string(schema, "description")) return false;
    if (!has_object(schema, "parameters")) return false;

    // 2. Parameters structure
    yyjson_val *params = get(schema, "parameters");
    if (!has_string_value(params, "type", "object")) return false;
    if (!has_object(params, "properties")) return false;

    // 3. Each property has valid type
    yyjson_val *props = get(params, "properties");
    for each property in props:
        if (!has_valid_type(property)) return false;
        // Recurse for nested objects

    // 4. Required array (if present) contains only property names
    if (has_array(params, "required")):
        for each name in required:
            if name not in properties: return false;

    return true;
}
```

## Validation Timing

| Step | What We Validate | When |
|------|------------------|------|
| Discovery | Tool returns valid JSON | `--schema` execution |
| Discovery | Schema has required structure | After JSON parse |
| Registration | Schema translates to all providers | Tool registration |
| Never | Input data matches schema | Providers validate this |

## Sources

- [yyjson GitHub](https://github.com/ibireme/yyjson)
- [jsonschema-c](https://github.com/helmut-jacob/jsonschema-c)
- [jsonc-daccord](https://github.com/domoslabs/jsonc-daccord)
- [RapidJSON Schema](https://rapidjson.org/md_doc_schema.html)
- [valijson](https://github.com/tristanpenman/valijson)
- [JSON Schema Tools](https://json-schema.org/tools)
