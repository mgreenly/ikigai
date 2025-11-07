// Protocol module implementation

#include "protocol.h"
#include "logger.h"
#include "wrapper.h"
#include <string.h>
#include <uuid/uuid.h>
#include <b64/cencode.h>

// Destructor for ik_protocol_msg_t - decrements JSON payload reference
static int
ik_protocol_msg_destructor (ik_protocol_msg_t *msg)
{
  if (msg->payload)
    {
      json_decref (msg->payload);
      msg->payload = NULL;
    }
  return 0;
}

ik_result_t
ik_protocol_msg_parse (TALLOC_CTX *ctx, const char *json_str)
{
  // Parse JSON
  json_error_t jerr;
  json_t *root = json_loads (json_str, 0, &jerr);
  if (!root)
    {
      return ERR (ctx, PARSE, "JSON parse error: %s", jerr.text);
    }

  // Verify root is an object
  if (ik_json_is_object_wrapper (root) == 0)
    {
      json_decref (root);
      return ERR (ctx, PARSE, "Root JSON is not an object");
    }

  // Allocate message on context
  ik_protocol_msg_t *msg = ik_talloc_zero_wrapper (ctx, sizeof (ik_protocol_msg_t));
  if (!msg)
    {
      json_decref (root);
      return ERR (ctx, OOM, "Failed to allocate message");
    }

  // Set destructor to clean up JSON payload
  talloc_set_destructor (msg, ik_protocol_msg_destructor);

  // Extract sess_id (required, string)
  json_t *sess_id_json = json_object_get (root, "sess_id");
  if (!sess_id_json)
    {
      json_decref (root);
      return ERR (ctx, PARSE, "Missing sess_id field");
    }
  if (ik_json_is_string_wrapper (sess_id_json) == 0)
    {
      json_decref (root);
      return ERR (ctx, PARSE, "Invalid sess_id field type");
    }
  msg->sess_id = ik_talloc_strdup_wrapper (msg, json_string_value (sess_id_json));
  if (!msg->sess_id)
    {
      json_decref (root);
      return ERR (ctx, OOM, "Failed to allocate sess_id");
    }

  // Extract corr_id (optional, string)
  json_t *corr_id_json = json_object_get (root, "corr_id");
  if (corr_id_json)
    {
      if (ik_json_is_string_wrapper (corr_id_json))
	{
	  msg->corr_id = ik_talloc_strdup_wrapper (msg, json_string_value (corr_id_json));
	  if (!msg->corr_id)
	    {
	      json_decref (root);
	      return ERR (ctx, OOM, "Failed to allocate corr_id");
	    }
	}
      else
	{
	  // Wrong type, treat as missing
	  msg->corr_id = NULL;
	}
    }
  else
    {
      msg->corr_id = NULL;
    }

  // Extract type (required, string)
  json_t *type_json = json_object_get (root, "type");
  if (!type_json)
    {
      json_decref (root);
      return ERR (ctx, PARSE, "Missing type field");
    }
  if (ik_json_is_string_wrapper (type_json) == 0)
    {
      json_decref (root);
      return ERR (ctx, PARSE, "Invalid type field type");
    }
  msg->type = ik_talloc_strdup_wrapper (msg, json_string_value (type_json));
  if (!msg->type)
    {
      json_decref (root);
      return ERR (ctx, OOM, "Failed to allocate type");
    }

  // Extract payload (required, object)
  json_t *payload_json = json_object_get (root, "payload");
  if (!payload_json)
    {
      json_decref (root);
      return ERR (ctx, PARSE, "Missing payload field");
    }
  if (ik_json_is_object_wrapper (payload_json) == 0)
    {
      json_decref (root);
      return ERR (ctx, PARSE, "Invalid payload field type");
    }

  // Increment reference count - caller now owns this json_t
  json_incref (payload_json);
  msg->payload = payload_json;

  // Decref root (payload ref count is now 1 after incref above)
  json_decref (root);

  return OK (msg);
}

ik_result_t
ik_protocol_msg_serialize (TALLOC_CTX *ctx, ik_protocol_msg_t *msg)
{
  // Create JSON object
  json_t *root = ik_json_object_wrapper ();
  if (!root)
    {
      return ERR (ctx, OOM, "Failed to create JSON object");
    }

  // Add fields
  json_object_set_new (root, "sess_id", json_string (msg->sess_id));

  // corr_id is optional
  if (msg->corr_id)
    {
      json_object_set_new (root, "corr_id", json_string (msg->corr_id));
    }

  json_object_set_new (root, "type", json_string (msg->type));

  // Add payload (reference, not copy)
  json_object_set (root, "payload", msg->payload);

  // Serialize to string
  char *json_str = ik_json_dumps_wrapper (root, JSON_COMPACT);
  json_decref (root);

  if (!json_str)
    {
      return ERR (ctx, OOM, "Failed to serialize JSON");
    }

  // Copy to talloc context
  char *result = ik_talloc_strdup_wrapper (ctx, json_str);
  free (json_str);		// jansson uses malloc

  if (!result)
    {
      return ERR (ctx, OOM, "Failed to allocate serialized string");
    }

  return OK (result);
}

ik_result_t
ik_protocol_generate_uuid (TALLOC_CTX *ctx)
{
  // Generate random UUID (16 bytes)
  uuid_t uuid;
  uuid_generate_random (uuid);

  // Base64 encode the UUID
  base64_encodestate encode_state;
  base64_init_encodestate (&encode_state);

  // Encode to temporary buffer (24 bytes base64 + padding + null)
  char b64_temp[32];
  int len = base64_encode_block ((const char *) uuid, sizeof (uuid), b64_temp, &encode_state);
  len += base64_encode_blockend (b64_temp + len, &encode_state);
  b64_temp[len] = '\0';

  // Convert base64 to base64url and allocate on talloc context
  // Remove padding and replace + with -, / with _
  char *b64url = ik_talloc_array_wrapper (ctx, sizeof (char), 23);	// 22 chars + null
  if (!b64url)
    {
      return ERR (ctx, OOM, "Failed to allocate UUID string");
    }

  int j = 0;
  for (int i = 0; b64_temp[i] && b64_temp[i] != '=' && j < 22; i++)	// LCOV_EXCL_BR_LINE - defensive bounds check and padding check, always 22 chars for UUID
    {
      if (b64_temp[i] == '+')
	{
	  b64url[j++] = '-';
	}
      else if (b64_temp[i] == '/')
	{
	  b64url[j++] = '_';
	}
      // LCOV_EXCL_START - libb64 behavior dependent, newlines not added in practice
      else if (b64_temp[i] == '\n' || b64_temp[i] == '\r')
	{
	  // Skip newlines that libb64 might add
	  continue;
	}
      // LCOV_EXCL_STOP
      else
	{
	  b64url[j++] = b64_temp[i];
	}
    }
  b64url[j] = '\0';

  return OK (b64url);
}

ik_result_t
ik_protocol_msg_create_err (TALLOC_CTX *ctx,
			    const char *sess_id, const char *corr_id, const char *source, const char *err_msg)
{
  // Allocate message on context
  ik_protocol_msg_t *msg = ik_talloc_zero_wrapper (ctx, sizeof (ik_protocol_msg_t));
  if (!msg)
    {
      return ERR (ctx, OOM, "Failed to allocate error message");
    }

  // Set destructor to clean up JSON payload
  talloc_set_destructor (msg, ik_protocol_msg_destructor);

  // Copy string fields
  msg->sess_id = ik_talloc_strdup_wrapper (msg, sess_id);
  if (!msg->sess_id)
    {
      return ERR (ctx, OOM, "Failed to allocate sess_id");
    }

  msg->corr_id = ik_talloc_strdup_wrapper (msg, corr_id);
  if (!msg->corr_id)		// LCOV_EXCL_BR_LINE - OOM path tested, but ERR macro interaction creates coverage artifact
    {
      return ERR (ctx, OOM, "Failed to allocate corr_id");	// LCOV_EXCL_LINE
    }

  msg->type = ik_talloc_strdup_wrapper (msg, "error");
  if (!msg->type)
    {
      return ERR (ctx, OOM, "Failed to allocate type");
    }

  // Create payload with source and message fields
  msg->payload = ik_json_object_wrapper ();
  if (!msg->payload)
    {
      return ERR (ctx, OOM, "Failed to create payload object");
    }

  json_object_set_new (msg->payload, "source", json_string (source));
  json_object_set_new (msg->payload, "message", json_string (err_msg));

  return OK (msg);
}

ik_result_t
ik_protocol_msg_create_assistant_resp (TALLOC_CTX *ctx, const char *sess_id, const char *corr_id, json_t *payload)
{
  // Allocate message on context
  ik_protocol_msg_t *msg = ik_talloc_zero_wrapper (ctx, sizeof (ik_protocol_msg_t));
  if (!msg)
    {
      return ERR (ctx, OOM, "Failed to allocate assistant response message");
    }

  // Set destructor to clean up JSON payload
  talloc_set_destructor (msg, ik_protocol_msg_destructor);

  // Copy string fields
  msg->sess_id = ik_talloc_strdup_wrapper (msg, sess_id);
  if (!msg->sess_id)
    {
      return ERR (ctx, OOM, "Failed to allocate sess_id");
    }

  msg->corr_id = ik_talloc_strdup_wrapper (msg, corr_id);
  if (!msg->corr_id)
    {
      return ERR (ctx, OOM, "Failed to allocate corr_id");
    }

  msg->type = ik_talloc_strdup_wrapper (msg, "assistant_response");
  if (!msg->type)		// LCOV_EXCL_BR_LINE - OOM path tested, but ERR macro interaction creates coverage artifact
    {
      return ERR (ctx, OOM, "Failed to allocate type");	// LCOV_EXCL_LINE
    }

  // Take ownership of payload (caller must not use/free it after this call)
  // Destructor will decrement the reference when message is freed
  msg->payload = payload;

  return OK (msg);
}
