#!/usr/bin/env -S deno run --allow-net --allow-read --allow-write

/**
 * apkg - Agent Package Manager
 *
 * Manages installation of agent packages (skills, scripts, commands) from the apkg repository.
 *
 * Subcommands:
 *   list              List all available packages
 *   installed         List currently installed files
 *   install <target>  Install a package or specific part
 *   update <target>   Update (re-download) a package or specific part
 *
 * Installation targets:
 *   <name>            Install all parts of a package (e.g., "coverage")
 *   <type>/<name>     Install a specific part (e.g., "skills/coverage")
 */

const MANIFEST_URL = "https://raw.githubusercontent.com/mgreenly/apkg/HEAD/manifest.json";
const REPO_BASE = "https://raw.githubusercontent.com/mgreenly/apkg/HEAD/agents";

// TypeScript interfaces
interface PackagePart {
  type: "skills" | "scripts" | "commands";
  name: string;
}

interface Package {
  name: string;
  description?: string;
  skills?: string[];
  scripts?: string[];
  commands?: string[];
}

interface Manifest {
  packages: Package[];
}

interface CommandResult {
  success: boolean;
  data?: unknown;
  error?: string;
  code?: string;
}

interface InstalledFile {
  source: string;
  destination: string;
  size: number;
}

// Manifest loading
async function loadManifest(): Promise<Manifest> {
  try {
    const response = await fetch(MANIFEST_URL);
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}: ${response.statusText}`);
    }
    const manifest = await response.json();
    return manifest as Manifest;
  } catch (error) {
    if (error instanceof Error) {
      throw new Error(`Failed to load manifest from ${MANIFEST_URL}: ${error.message}`);
    }
    throw error;
  }
}

// List command
async function listCommand(verbose: boolean): Promise<CommandResult> {
  try {
    const manifest = await loadManifest();

    const packageList = manifest.packages.map(pkg => {
      const parts: string[] = [];
      if (pkg.skills) parts.push(...pkg.skills.map(s => `skills/${s}`));
      if (pkg.scripts) parts.push(...pkg.scripts.map(s => `scripts/${s}`));
      if (pkg.commands) parts.push(...pkg.commands.map(s => `commands/${s}`));

      return {
        name: pkg.name,
        description: pkg.description || "",
        parts: parts,
        partCount: parts.length,
      };
    });

    if (verbose) {
      return {
        success: true,
        data: { packages: packageList },
      };
    } else {
      return {
        success: true,
        data: {
          packages: packageList.map(p => ({
            name: p.name,
            description: p.description,
            partCount: p.partCount,
          })),
        },
      };
    }
  } catch (error) {
    return {
      success: false,
      error: error instanceof Error ? error.message : String(error),
      code: "LIST_FAILED",
    };
  }
}

// Parse installation target
function parseTarget(target: string): { mode: "package" | "part"; name: string; type?: string } {
  if (target.includes("/")) {
    const parts = target.split("/");
    if (parts.length !== 2) {
      throw new Error(`Invalid target format: ${target}. Expected <type>/<name> or <name>`);
    }
    return { mode: "part", type: parts[0], name: parts[1] };
  }
  return { mode: "package", name: target };
}

// Download a file from the repository
async function downloadFile(type: string, name: string): Promise<string> {
  const url = `${REPO_BASE}/${type}/${name}`;
  const response = await fetch(url);
  if (!response.ok) {
    throw new Error(`HTTP ${response.status}: ${response.statusText}`);
  }
  return await response.text();
}

// Install a single file
async function installFile(type: string, name: string, overwrite: boolean): Promise<InstalledFile> {
  const content = await downloadFile(type, name);
  const destination = `/home/ai4mgreenly/projects/ikigai/main/.agents/${type}/${name}`;

  // Create directory if needed
  const dir = destination.substring(0, destination.lastIndexOf("/"));
  try {
    await Deno.mkdir(dir, { recursive: true });
  } catch (error) {
    if (!(error instanceof Deno.errors.AlreadyExists)) {
      throw error;
    }
  }

  // Check if file exists
  if (!overwrite) {
    try {
      await Deno.stat(destination);
      // File exists, don't overwrite
      return {
        source: `${REPO_BASE}/${type}/${name}`,
        destination,
        size: 0, // Not written
      };
    } catch {
      // File doesn't exist, proceed
    }
  }

  await Deno.writeTextFile(destination, content);
  const size = content.length;

  return {
    source: `${REPO_BASE}/${type}/${name}`,
    destination,
    size,
  };
}

// Install command
async function installCommand(target: string, overwrite = false): Promise<CommandResult> {
  try {
    const manifest = await loadManifest();
    const parsed = parseTarget(target);
    const installedFiles: InstalledFile[] = [];

    if (parsed.mode === "package") {
      // Install all parts of a package
      const pkg = manifest.packages.find(p => p.name === parsed.name);
      if (!pkg) {
        return {
          success: false,
          error: `Package not found: ${parsed.name}`,
          code: "PACKAGE_NOT_FOUND",
        };
      }

      // Install all parts
      const parts: PackagePart[] = [];
      if (pkg.skills) parts.push(...pkg.skills.map(s => ({ type: "skills" as const, name: s })));
      if (pkg.scripts) parts.push(...pkg.scripts.map(s => ({ type: "scripts" as const, name: s })));
      if (pkg.commands) parts.push(...pkg.commands.map(s => ({ type: "commands" as const, name: s })));

      for (const part of parts) {
        try {
          const file = await installFile(part.type, part.name, overwrite);
          installedFiles.push(file);
        } catch (error) {
          return {
            success: false,
            error: `Failed to install ${part.type}/${part.name}: ${error instanceof Error ? error.message : String(error)}`,
            code: "INSTALL_FAILED",
          };
        }
      }
    } else {
      // Install a specific part
      if (!parsed.type || !["skills", "scripts", "commands"].includes(parsed.type)) {
        return {
          success: false,
          error: `Invalid part type: ${parsed.type}. Must be skills, scripts, or commands`,
          code: "INVALID_TYPE",
        };
      }

      try {
        const file = await installFile(parsed.type, parsed.name, overwrite);
        installedFiles.push(file);
      } catch (error) {
        return {
          success: false,
          error: `Failed to install ${parsed.type}/${parsed.name}: ${error instanceof Error ? error.message : String(error)}`,
          code: "INSTALL_FAILED",
        };
      }
    }

    return {
      success: true,
      data: {
        target,
        mode: parsed.mode,
        installed: installedFiles,
      },
    };
  } catch (error) {
    return {
      success: false,
      error: error instanceof Error ? error.message : String(error),
      code: "INSTALL_FAILED",
    };
  }
}

// Update command (same as install but with overwrite)
async function updateCommand(target: string): Promise<CommandResult> {
  return await installCommand(target, true);
}

// Installed command - list what's currently installed
async function installedCommand(): Promise<CommandResult> {
  try {
    const agentsDir = "/home/ai4mgreenly/projects/ikigai/main/.agents";
    const types = ["skills", "scripts", "commands"] as const;
    const installed: Record<string, string[]> = {
      skills: [],
      scripts: [],
      commands: [],
    };

    for (const type of types) {
      const typeDir = `${agentsDir}/${type}`;
      try {
        for await (const entry of Deno.readDir(typeDir)) {
          if (entry.isFile && entry.name.endsWith(".md")) {
            installed[type].push(entry.name);
          } else if (entry.isDirectory && type === "scripts") {
            // For scripts, include directory names
            installed[type].push(entry.name);
          }
        }
      } catch (error) {
        // Directory might not exist, that's ok
        if (!(error instanceof Deno.errors.NotFound)) {
          throw error;
        }
      }
    }

    // Sort each type
    for (const type of types) {
      installed[type].sort();
    }

    return {
      success: true,
      data: {
        installed,
        total: installed.skills.length + installed.scripts.length + installed.commands.length,
      },
    };
  } catch (error) {
    return {
      success: false,
      error: error instanceof Error ? error.message : String(error),
      code: "INSTALLED_FAILED",
    };
  }
}

// Show help
function showHelp(): void {
  console.error("apkg - Agent Package Manager");
  console.error("");
  console.error("Usage: apkg <subcommand> [options]");
  console.error("");
  console.error("Subcommands:");
  console.error("  list              List all available packages");
  console.error("  list --verbose    List packages with detailed part information");
  console.error("  installed         List currently installed files");
  console.error("  install <target>  Install a package or specific part");
  console.error("  update <target>   Update (re-download) a package or specific part");
  console.error("");
  console.error("Installation targets:");
  console.error("  <name>            Install all parts of a package (e.g., 'coverage')");
  console.error("  <type>/<name>     Install a specific part (e.g., 'skills/coverage')");
  console.error("");
  console.error("Examples:");
  console.error("  apkg list");
  console.error("  apkg list --verbose");
  console.error("  apkg install coverage");
  console.error("  apkg install skills/coverage");
  console.error("  apkg update coverage");
  console.error("");
  console.error("All commands return JSON output:");
  console.error("  Success: {success: true, data: {...}}");
  console.error("  Error:   {success: false, error: string, code: string}");
}

// Main function
async function main(): Promise<void> {
  const args = Deno.args;

  if (args.length === 0 || args.includes("--help") || args.includes("-h")) {
    showHelp();
    Deno.exit(args.length === 0 ? 1 : 0);
  }

  const subcommand = args[0];
  let result: CommandResult;

  switch (subcommand) {
    case "list": {
      const verbose = args.includes("--verbose") || args.includes("-v");
      result = await listCommand(verbose);
      break;
    }

    case "installed": {
      result = await installedCommand();
      break;
    }

    case "install": {
      if (args.length < 2) {
        result = {
          success: false,
          error: "Missing target argument. Usage: apkg install <target>",
          code: "MISSING_ARGUMENT",
        };
      } else {
        result = await installCommand(args[1]);
      }
      break;
    }

    case "update": {
      if (args.length < 2) {
        result = {
          success: false,
          error: "Missing target argument. Usage: apkg update <target>",
          code: "MISSING_ARGUMENT",
        };
      } else {
        result = await updateCommand(args[1]);
      }
      break;
    }

    default:
      result = {
        success: false,
        error: `Invalid subcommand: ${subcommand}. Valid subcommands: list, installed, install, update`,
        code: "INVALID_SUBCOMMAND",
      };
  }

  console.log(JSON.stringify(result, null, 2));
  Deno.exit(result.success ? 0 : 1);
}

if (import.meta.main) {
  await main();
}
