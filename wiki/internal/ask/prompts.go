package ask

import "strings"

func analysisPrompt(question string) string {
	return "Prepare the wiki question for retrieval. " +
		"Return only JSON with sub_queries, keywords, and aliases arrays. " +
		"Split sub_queries by subject and return at most four. " +
		"Use keywords for salient terms and aliases for alternate names.\n\nQuestion: " +
		strings.TrimSpace(question)
}
