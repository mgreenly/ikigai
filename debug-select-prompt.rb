#!/usr/bin/env ruby
# frozen_string_literal: true

require 'json'
require 'erb'

# Same code as ralph/run
def load_requirements(requirements_file)
  JSON.parse(File.read(requirements_file))['requirements']
end

def render_prompt(template_file, vars = {})
  template = File.read(template_file)
  ERB.new(template).result_with_hash(vars)
end

# Config
requirements_file = 'rel-08/ralph-code-removal.json'
history_file = '.claude/harness/ralph/history.jsonl'
template_file = '.claude/harness/ralph/select.md.erb'

# Gather pending requirements (same as ralph)
requirements = load_requirements(requirements_file)
pending = requirements.select { |r| r['status'] == 'pending' }
                      .map { |r| { id: r['id'], requirement: r['requirement'] } }

# Get recent history
history_lines = File.exist?(history_file) ? File.readlines(history_file).last(10) : []

# Render the prompt
prompt_content = render_prompt(template_file, {
  pending: pending,
  history: history_lines.join
})

puts prompt_content
