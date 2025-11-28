# Why JSON-Based Projection with Dual Format Output?

**Decision**: Use JSON (via json-c) as the intermediate representation for record projections, with direct serialization to both JSON and YAML formats.

## Context

The application needs to:
- Display filtered subsets of complex internal records (100-500 lines of JSON when fully serialized)
- Support multiple views of the same data (summary, detailed, full, debug)
- Output to CLI/terminal for human operators
- Filters are known at compile time (fixed, predetermined views)
- Records have moderate complexity with moderate nesting

## Decision

Implement a two-layer architecture:

**Layer 1 - Projection**: Convert internal records to json-c objects with view-specific field selection
```c
json_object* record_to_summary(const Record* r);   // 5-20 fields
json_object* record_to_detailed(const Record* r);  // 30-50 fields
json_object* record_to_full(const Record* r);      // All fields
```

**Layer 2 - Serialization**: Convert json-c objects to output format
```c
void print_json(FILE* out, json_object* obj, bool pretty);
void print_yaml(FILE* out, json_object* obj);
```

## Rationale

### Why Build Filtered Subsets Directly?
- **Memory efficiency**: Building 500-line records just to display 20 lines wastes memory
- **Performance**: Constructing unused fields has measurable overhead
- **Clarity**: Each view's requirements are explicit in code
- **Type safety**: Compile-time validation of field access

### Why JSON as Intermediate Format?
- **Single projection logic**: Each view defined once, works for both output formats
- **Well-tested library**: json-c is mature, widely used, efficient
- **Natural fit**: Most internal data maps cleanly to JSON types
- **Composability**: Projection separated from serialization
- **Testable**: Can test projection logic independent of output format

### Why Support Both JSON and YAML?
- **YAML for humans**: More readable in terminal (less noise, better for deep nesting)
- **JSON for machines**: Piping to jq, scripting, automation
- **Flexibility**: Users choose format based on context

## Implementation

### Libraries
```bash
sudo apt install libjson-c-dev libyaml-dev
```

### Directory Structure
```
src/
  projection.h      # Projection functions (record → json-c)
  projection.c
  formatter.h       # Serialization functions (json-c → output)
  formatter.c
```

### Usage Pattern
```c
// In application code:
json_object* view = record_to_summary(record);

if (output_format == FORMAT_JSON) {
    print_json(stdout, view, pretty);
} else {
    print_yaml(stdout, view);
}

json_object_put(view);  // Cleanup
```

### CLI Interface
```bash
./ikigai show client 123                  # YAML summary (default)
./ikigai show client 123 --full          # YAML full
./ikigai show client 123 --json          # JSON summary
./ikigai show client 123 --json --full   # JSON full
./ikigai show client 123 --json | jq .   # Pipe to tools
```

## Alternatives Considered

### 1. Build Full Record → Filter with jq
```c
json_object* full = record_to_json(r);
// Apply jq filter...
```

**Rejected because**:
- Memory waste (building 500 lines to show 20)
- Two-pass processing overhead
- Adds jq dependency for compile-time filtering
- Better suited for dynamic/runtime filtering (not our use case)

### 2. Separate View Structs as Intermediate
```c
SummaryView* view = record_to_summary_view(r);
json_object* json = summary_view_to_json(view);
```

**Rejected because**:
- Triples representations: Record → ViewStruct → JSON/YAML
- Extra struct definitions to maintain
- Additional memory allocations
- Unclear benefit over using json-c directly

### 3. Duplicate Projection Logic Per Format
```c
json_object* record_to_json_summary(const Record* r);
yaml_document_t* record_to_yaml_summary(const Record* r);
```

**Rejected because**:
- Code duplication (every view × every format)
- Inconsistency risk (forgetting to update one)
- Double the maintenance burden

### 4. Generic Key-Value Intermediate Format
```c
typedef struct { char* key; void* value; int type; } Field;
Field* record_to_summary(const Record* r);
```

**Rejected because**:
- Type-unsafe in C
- Reinventing json-c
- More complex than using established library

## Trade-offs

### Pros
✅ **Efficient**: One-pass, build only needed fields
✅ **Composable**: Projection separate from serialization
✅ **Maintainable**: Each view in one place
✅ **Testable**: Can unit test projections
✅ **Flexible**: Both output formats from single projection
✅ **Clear**: Explicit view requirements
✅ **Standard libraries**: json-c and libyaml are mature

### Cons
❌ **JSON dependency**: All projections go through json-c (even for YAML output)
❌ **Two libraries**: Need both json-c and libyaml
❌ **No dynamic filtering**: Can't do arbitrary jq-style queries (but we don't need this)
❌ **Type mapping**: C types → JSON types → YAML types (small overhead)

## Future Considerations

### If Requirements Change:

**If dynamic filtering becomes needed:**
- Can add jq integration as Layer 3
- Apply jq filters to json-c objects before serialization
- Backwards compatible (just adds capability)

**If other output formats needed (XML, MessagePack, etc.):**
- Add new serializer in Layer 2
- Projections remain unchanged
- Pattern scales naturally

**If performance critical:**
- Can optimize specific views with direct serialization
- But keep pattern for non-critical paths
- Measure first before optimizing

## Terminology

- **Projection**: Selecting a subset of fields from a record
- **View**: A named projection (summary, detailed, full)
- **Serialization**: Converting to output format (JSON, YAML)
- **Formatter**: The serialization function

## References

- json-c documentation: https://json-c.github.io/json-c/
- libyaml documentation: https://pyyaml.org/wiki/LibYAML
- Related decisions:
  - [link-seams-mocking.md](link-seams-mocking.md) - Wrappers will be needed for json-c and libyaml
