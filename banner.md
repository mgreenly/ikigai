# Ikigai 12/12 Dev Stream

  * https://github.com/mgreenly/ikigai

Tonight we're refactoring ikigai to support multiple simultaneous agents.
The first step is to refactor the monolithic context into shared_ctx
(terminal/render) and agent_ctx (conversation/scrollback). So no new
features tonight, just restructuring code to better support many agents.
