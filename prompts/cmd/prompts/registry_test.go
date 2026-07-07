package main

import (
	"fmt"
	"go/ast"
	"go/parser"
	"go/token"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
	"testing"

	"registry"
)

func TestRegistryProvidesPromptsPortAndSpecUsesIt(t *testing.T) {
	// R-RG01-PORT
	if got := registry.MustPort("prompts"); got != 3002 {
		t.Fatalf(`registry.MustPort("prompts") = %d, want 3002`, got)
	}

	expr := appkitSpecPortExpr(t)
	if !isRegistryMustPortCall(expr, "prompts") {
		t.Fatalf("appkit.Spec.Port is %s, want registry.MustPort(%q)", exprString(t, expr), "prompts")
	}
	if containsIntLiteral(expr, "3002") {
		t.Fatalf("appkit.Spec.Port contains literal 3002: %s", exprString(t, expr))
	}
}

func TestDropboxBaseURLDefaultsThroughRegistryAndCanBeOverridden(t *testing.T) {
	// R-RG03-DBOX
	if got, want := dropboxBaseURL(func(string) string { return "" }), registry.BaseURL("dropbox"); got != want {
		t.Fatalf("dropboxBaseURL(unset) = %q, want %q", got, want)
	}
	if got, want := dropboxBaseURL(func(string) string { return "" }), "http://127.0.0.1:3200"; got != want {
		t.Fatalf("dropboxBaseURL(unset) = %q, want concrete default %q", got, want)
	}

	const override = "http://127.0.0.1:9999"
	got := dropboxBaseURL(func(key string) string {
		if key == "DROPBOX_BASE_URL" {
			return override
		}
		return ""
	})
	if got != override {
		t.Fatalf("dropboxBaseURL(override) = %q, want %q", got, override)
	}
}

func TestNonTestGoFilesDoNotHardCodeLoopbackRegistryPorts(t *testing.T) {
	// R-RG04-NLIT
	moduleRoot := filepath.Join("..", "..")
	forbidden := regexp.MustCompile(`127\.0\.0\.1:30|\b3002\b`)

	err := filepath.WalkDir(moduleRoot, func(path string, entry os.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if entry.IsDir() {
			switch entry.Name() {
			case ".git", "tmp":
				return filepath.SkipDir
			default:
				return nil
			}
		}
		if filepath.Ext(path) != ".go" || strings.HasSuffix(path, "_test.go") {
			return nil
		}

		data, err := os.ReadFile(path)
		if err != nil {
			return err
		}
		if match := forbidden.Find(data); match != nil {
			rel, relErr := filepath.Rel(moduleRoot, path)
			if relErr != nil {
				rel = path
			}
			t.Fatalf("%s contains hard-coded loopback registry port fragment %q", rel, match)
		}
		return nil
	})
	if err != nil {
		t.Fatalf("walk Go files: %v", err)
	}
}

func appkitSpecPortExpr(t *testing.T) ast.Expr {
	t.Helper()

	fset := token.NewFileSet()
	file, err := parser.ParseFile(fset, "main.go", nil, 0)
	if err != nil {
		t.Fatalf("parse main.go: %v", err)
	}

	var port ast.Expr
	ast.Inspect(file, func(node ast.Node) bool {
		if port != nil {
			return false
		}
		lit, ok := node.(*ast.CompositeLit)
		if !ok || !isSelector(lit.Type, "appkit", "Spec") {
			return true
		}
		for _, elt := range lit.Elts {
			kv, ok := elt.(*ast.KeyValueExpr)
			if !ok {
				continue
			}
			key, ok := kv.Key.(*ast.Ident)
			if ok && key.Name == "Port" {
				port = kv.Value
				return false
			}
		}
		return true
	})
	if port == nil {
		t.Fatal("appkit.Spec.Port not found in main.go")
	}
	return port
}

func isRegistryMustPortCall(expr ast.Expr, service string) bool {
	call, ok := expr.(*ast.CallExpr)
	if !ok || !isSelector(call.Fun, "registry", "MustPort") || len(call.Args) != 1 {
		return false
	}
	lit, ok := call.Args[0].(*ast.BasicLit)
	if !ok || lit.Kind != token.STRING {
		return false
	}
	got, err := strconv.Unquote(lit.Value)
	return err == nil && got == service
}

func isSelector(expr ast.Expr, pkg, name string) bool {
	sel, ok := expr.(*ast.SelectorExpr)
	if !ok || sel.Sel.Name != name {
		return false
	}
	id, ok := sel.X.(*ast.Ident)
	return ok && id.Name == pkg
}

func containsIntLiteral(expr ast.Expr, literal string) bool {
	var found bool
	ast.Inspect(expr, func(node ast.Node) bool {
		if found {
			return false
		}
		lit, ok := node.(*ast.BasicLit)
		if ok && lit.Kind == token.INT && lit.Value == literal {
			found = true
			return false
		}
		return true
	})
	return found
}

func exprString(t *testing.T, expr ast.Expr) string {
	t.Helper()

	switch v := expr.(type) {
	case *ast.CallExpr:
		if sel, ok := v.Fun.(*ast.SelectorExpr); ok {
			if id, ok := sel.X.(*ast.Ident); ok {
				args := make([]string, 0, len(v.Args))
				for _, arg := range v.Args {
					if lit, ok := arg.(*ast.BasicLit); ok {
						args = append(args, lit.Value)
					} else {
						args = append(args, "<expr>")
					}
				}
				return id.Name + "." + sel.Sel.Name + "(" + strings.Join(args, ", ") + ")"
			}
		}
	case *ast.BasicLit:
		return v.Value
	}
	return astNodeType(expr)
}

func astNodeType(node ast.Node) string {
	return strings.TrimPrefix(fmt.Sprintf("%T", node), "*ast.")
}
