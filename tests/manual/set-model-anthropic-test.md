# Set Model to Anthropic Claude

## Preconditions
- ikigai is running

## Test: claude-haiku-4-5

### Steps
1. `ikigai-ctl send_keys "/clear\r"`
2. `ikigai-ctl send_keys "/model claude-haiku-4-5/low\r"`
3. `ikigai-ctl read_framebuffer`
4. `ikigai-ctl send_keys "Hello\r"`
5. Wait for response, then `ikigai-ctl read_framebuffer`

### Expected
- Framebuffer contains a message indicating the model was switched
- Framebuffer contains a message indicating low thinking was set
- Model indicator at the bottom of the screen shows "🤖 claude-haiku-4-5/low"
- Framebuffer contains a line starting with "●" indicating a response from the model

## Test: claude-sonnet-4-5

### Steps
1. `ikigai-ctl send_keys "/clear\r"`
2. `ikigai-ctl send_keys "/model claude-sonnet-4-5/low\r"`
3. `ikigai-ctl read_framebuffer`
4. `ikigai-ctl send_keys "Hello\r"`
5. Wait for response, then `ikigai-ctl read_framebuffer`

### Expected
- Framebuffer contains a message indicating the model was switched
- Framebuffer contains a message indicating low thinking was set
- Model indicator at the bottom of the screen shows "🤖 claude-sonnet-4-5/low"
- Framebuffer contains a line starting with "●" indicating a response from the model

## Test: claude-opus-4-5

### Steps
1. `ikigai-ctl send_keys "/clear\r"`
2. `ikigai-ctl send_keys "/model claude-opus-4-5/low\r"`
3. `ikigai-ctl read_framebuffer`
4. `ikigai-ctl send_keys "Hello\r"`
5. Wait for response, then `ikigai-ctl read_framebuffer`

### Expected
- Framebuffer contains a message indicating the model was switched
- Framebuffer contains a message indicating low thinking was set
- Model indicator at the bottom of the screen shows "🤖 claude-opus-4-5/low"
- Framebuffer contains a line starting with "●" indicating a response from the model

## Test: claude-opus-4-6

### Steps
1. `ikigai-ctl send_keys "/clear\r"`
2. `ikigai-ctl send_keys "/model claude-opus-4-6/low\r"`
3. `ikigai-ctl read_framebuffer`
4. `ikigai-ctl send_keys "Hello\r"`
5. Wait for response, then `ikigai-ctl read_framebuffer`

### Expected
- Framebuffer contains a message indicating the model was switched
- Framebuffer contains a message indicating low thinking was set
- Model indicator at the bottom of the screen shows "🤖 claude-opus-4-6/low"
- Framebuffer contains a line starting with "●" indicating a response from the model
