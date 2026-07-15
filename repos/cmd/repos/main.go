// Command repos provisions GitHub repositories and runs autonomous sessions on
// the shared appkit chassis.
package main

import "appkit"

func main() {
	appkit.Main(reposSpec())
}
