const PORT = 8889;

Deno.serve({ port: PORT }, (req: Request): Response => {
  const url = new URL(req.url);
  const start = Date.now();

  let response: Response;
  if (url.pathname === "/api/hello") {
    response = new Response(JSON.stringify({ message: "Hello from Deno backend!" }), {
      headers: { "Content-Type": "application/json" },
    });
  } else {
    response = new Response("Not Found", { status: 404 });
  }

  console.log(`${req.method} ${url.pathname} ${response.status} ${Date.now() - start}ms`);
  return response;
});

console.log(`Backend running on port ${PORT}`);
