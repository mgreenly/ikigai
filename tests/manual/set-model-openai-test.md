# Set Model to OpenAI GPT

## Preconditions
- ikigai is running

## Test: gpt-5.2

### Steps
1. `ikigai-ctl send_keys "/clear\r"`
2. `ikigai-ctl send_keys "/model gpt-5.2/low\r"`
3. `ikigai-ctl read_framebuffer`
4. `ikigai-ctl send_keys "Hello\r"`
5. Wait for response, then `ikigai-ctl read_framebuffer`

### Expected
- Framebuffer contains a message indicating the model was switched
- Framebuffer contains a message indicating "thinking: low effort"
- Model indicator at the bottom of the screen shows "🤖 gpt-5.2/low"
- Framebuffer contains a line starting with "●" indicating a response from the model

## Test: gpt-5.2-codex

### Steps
1. `ikigai-ctl send_keys "/clear\r"`
2. `ikigai-ctl send_keys "/model gpt-5.2-codex/low\r"`
3. `ikigai-ctl read_framebuffer`
4. `ikigai-ctl send_keys "Hello\r"`
5. Wait for response, then `ikigai-ctl read_framebuffer`

### Expected
- Framebuffer contains a message indicating the model was switched
- Framebuffer contains a message indicating "thinking: low effort"
- Model indicator at the bottom of the screen shows "🤖 gpt-5.2-codex/low"
- Framebuffer contains a line starting with "●" indicating a response from the model

## Test: gpt-5-mini

### Steps
1. `ikigai-ctl send_keys "/clear\r"`
2. `ikigai-ctl send_keys "/model gpt-5-mini/low\r"`
3. `ikigai-ctl read_framebuffer`
4. `ikigai-ctl send_keys "Hello\r"`
5. Wait for response, then `ikigai-ctl read_framebuffer`

### Expected
- Framebuffer contains a message indicating the model was switched
- Framebuffer contains a message indicating "thinking: low effort"
- Model indicator at the bottom of the screen shows "🤖 gpt-5-mini/low"
- Framebuffer contains a line starting with "●" indicating a response from the model

## Test: gpt-5-nano

### Steps
1. `ikigai-ctl send_keys "/clear\r"`
2. `ikigai-ctl send_keys "/model gpt-5-nano/low\r"`
3. `ikigai-ctl read_framebuffer`
4. `ikigai-ctl send_keys "Hello\r"`
5. Wait for response, then `ikigai-ctl read_framebuffer`

### Expected
- Framebuffer contains a message indicating the model was switched
- Framebuffer contains a message indicating "thinking: low effort"
- Model indicator at the bottom of the screen shows "🤖 gpt-5-nano/low"
- Framebuffer contains a line starting with "●" indicating a response from the model
