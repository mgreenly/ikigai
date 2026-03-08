/**
 * @file internal_tool_skill_register.c
 * @brief Schema definitions and registration for skill management internal tools
 */

#include "apps/ikigai/internal_tool_skill.h"

#include "apps/ikigai/tool_registry.h"
#include "shared/json_allocator.h"
#include "shared/panic.h"
#include "vendor/yyjson/yyjson.h"

#include <string.h>
#include <talloc.h>

#include "shared/poison.h"

static const char *LOAD_SKILL_SCHEMA =
    "{"
    "  \"name\": \"load_skill\","
    "  \"description\": \"Load a skill by name into the current agent context. Skills add domain knowledge to the system prompt.\","
    "  \"parameters\": {"
    "    \"type\": \"object\","
    "    \"properties\": {"
    "      \"skill\": {"
    "        \"type\": \"string\","
    "        \"description\": \"Skill name to load (e.g., 'database', 'style')\""
    "      },"
    "      \"args\": {"
    "        \"type\": \"array\","
    "        \"items\": {\"type\": \"string\"},"
    "        \"description\": \"Optional positional arguments for ${1}, ${2}, etc. substitution\""
    "      }"
    "    },"
    "    \"required\": [\"skill\"]"
    "  }"
    "}";

static const char *UNLOAD_SKILL_SCHEMA =
    "{"
    "  \"name\": \"unload_skill\","
    "  \"description\": \"Remove a previously loaded skill from the current agent context.\","
    "  \"parameters\": {"
    "    \"type\": \"object\","
    "    \"properties\": {"
    "      \"skill\": {"
    "        \"type\": \"string\","
    "        \"description\": \"Skill name to unload\""
    "      }"
    "    },"
    "    \"required\": [\"skill\"]"
    "  }"
    "}";

static const char *LOAD_SKILLSET_SCHEMA =
    "{"
    "  \"name\": \"load_skillset\","
    "  \"description\": \"Load a skillset: preloads selected skills and advertises others in the catalog.\","
    "  \"parameters\": {"
    "    \"type\": \"object\","
    "    \"properties\": {"
    "      \"skillset\": {"
    "        \"type\": \"string\","
    "        \"description\": \"Skillset name (e.g., 'developer', 'architect')\""
    "      }"
    "    },"
    "    \"required\": [\"skillset\"]"
    "  }"
    "}";

static const char *LIST_SKILLS_SCHEMA =
    "{"
    "  \"name\": \"list_skills\","
    "  \"description\": \"List currently loaded skills and available skill catalog entries.\","
    "  \"parameters\": {"
    "    \"type\": \"object\","
    "    \"properties\": {},"
    "    \"required\": []"
    "  }"
    "}";

void ik_skill_tools_register(ik_tool_registry_t *registry)
{
    TALLOC_CTX *tmp_ctx = talloc_new(NULL);
    if (tmp_ctx == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_alc allocator = ik_make_talloc_allocator(registry);

    char *ls_buf = talloc_strdup(tmp_ctx, LOAD_SKILL_SCHEMA);
    if (!ls_buf) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_doc *ls_doc = yyjson_read_opts(ls_buf, strlen(ls_buf), 0,
                                          &allocator, NULL);
    if (!ls_doc) PANIC("Failed to parse load_skill schema");  // LCOV_EXCL_BR_LINE

    char *us_buf = talloc_strdup(tmp_ctx, UNLOAD_SKILL_SCHEMA);
    if (!us_buf) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_doc *us_doc = yyjson_read_opts(us_buf, strlen(us_buf), 0,
                                          &allocator, NULL);
    if (!us_doc) PANIC("Failed to parse unload_skill schema");  // LCOV_EXCL_BR_LINE

    char *lss_buf = talloc_strdup(tmp_ctx, LOAD_SKILLSET_SCHEMA);
    if (!lss_buf) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_doc *lss_doc = yyjson_read_opts(lss_buf, strlen(lss_buf), 0,
                                           &allocator, NULL);
    if (!lss_doc) PANIC("Failed to parse load_skillset schema");  // LCOV_EXCL_BR_LINE

    char *li_buf = talloc_strdup(tmp_ctx, LIST_SKILLS_SCHEMA);
    if (!li_buf) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_doc *li_doc = yyjson_read_opts(li_buf, strlen(li_buf), 0,
                                          &allocator, NULL);
    if (!li_doc) PANIC("Failed to parse list_skills schema");  // LCOV_EXCL_BR_LINE

    res_t res = ik_tool_registry_add_internal(
        registry, "load_skill", ls_doc,
        ik_internal_tool_load_skill_handler,
        ik_internal_tool_load_skill_on_complete);
    if (is_err(&res)) PANIC("Failed to register load_skill tool");  // LCOV_EXCL_BR_LINE

    res = ik_tool_registry_add_internal(
        registry, "unload_skill", us_doc,
        ik_internal_tool_unload_skill_handler,
        ik_internal_tool_unload_skill_on_complete);
    if (is_err(&res)) PANIC("Failed to register unload_skill tool");  // LCOV_EXCL_BR_LINE

    res = ik_tool_registry_add_internal(
        registry, "load_skillset", lss_doc,
        ik_internal_tool_load_skillset_handler,
        ik_internal_tool_load_skillset_on_complete);
    if (is_err(&res)) PANIC("Failed to register load_skillset tool");  // LCOV_EXCL_BR_LINE

    res = ik_tool_registry_add_internal(
        registry, "list_skills", li_doc,
        ik_internal_tool_list_skills_handler, NULL);
    if (is_err(&res)) PANIC("Failed to register list_skills tool");  // LCOV_EXCL_BR_LINE

    talloc_free(tmp_ctx);
}
