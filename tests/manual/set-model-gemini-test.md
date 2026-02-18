# Set Model to Google Gemini

## Preconditions
- ikigai is running

## Test: gemini-2.5-flash-lite

### Steps
1. `ikigai-ctl send_keys "/clear\r"`
2. `ikigai-ctl send_keys "/model gemini-2.5-flash-lite/low\r"`
3. `ikigai-ctl read_framebuffer`
4. `ikigai-ctl send_keys "Hello\r"`
5. Wait for response, then `ikigai-ctl read_framebuffer`

### Expected
- Framebuffer contains a message indicating the model was switched
- Framebuffer contains a message indicating low thinking was set
- Model indicator at the bottom of the screen shows "🤖 gemini-2.5-flash-lite/low"
- Framebuffer contains a line starting with "●" indicating a response from the model

## Test: gemini-2.5-flash

### Steps
1. `ikigai-ctl send_keys "/clear\r"`
2. `ikigai-ctl send_keys "/model gemini-2.5-flash/low\r"`
3. `ikigai-ctl read_framebuffer`
4. `ikigai-ctl send_keys "Hello\r"`
5. Wait for response, then `ikigai-ctl read_framebuffer`

### Expected
- Framebuffer contains a message indicating the model was switched
- Framebuffer contains a message indicating low thinking was set
- Model indicator at the bottom of the screen shows "🤖 gemini-2.5-flash/low"
- Framebuffer contains a line starting with "●" indicating a response from the model

## Test: gemini-2.5-pro

### Steps
1. `ikigai-ctl send_keys "/clear\r"`
2. `ikigai-ctl send_keys "/model gemini-2.5-pro/low\r"`
3. `ikigai-ctl read_framebuffer`
4. `ikigai-ctl send_keys "Hello\r"`
5. Wait for response, then `ikigai-ctl read_framebuffer`

### Expected
- Framebuffer contains a message indicating the model was switched
- Framebuffer contains a message indicating low thinking was set
- Model indicator at the bottom of the screen shows "🤖 gemini-2.5-pro/low"
- Framebuffer contains a line starting with "●" indicating a response from the model

## Test: gemini-3-flash-preview

### Steps
1. `ikigai-ctl send_keys "/clear\r"`
2. `ikigai-ctl send_keys "/model gemini-3-flash-preview/low\r"`
3. `ikigai-ctl read_framebuffer`
4. `ikigai-ctl send_keys "Hello\r"`
5. Wait for response, then `ikigai-ctl read_framebuffer`

### Expected
- Framebuffer contains a message indicating the model was switched
- Framebuffer contains a message indicating low thinking was set
- Model indicator at the bottom of the screen shows "🤖 gemini-3-flash-preview/low"
- Framebuffer contains a line starting with "●" indicating a response from the model

## Test: gemini-3-pro-preview

### Steps
1. `ikigai-ctl send_keys "/clear\r"`
2. `ikigai-ctl send_keys "/model gemini-3-pro-preview/low\r"`
3. `ikigai-ctl read_framebuffer`
4. `ikigai-ctl send_keys "Hello\r"`
5. Wait for response, then `ikigai-ctl read_framebuffer`

### Expected
- Framebuffer contains a message indicating the model was switched
- Framebuffer contains a message indicating low thinking was set
- Model indicator at the bottom of the screen shows "🤖 gemini-3-pro-preview/low"
- Framebuffer contains a line starting with "●" indicating a response from the model
