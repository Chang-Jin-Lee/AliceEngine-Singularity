// The exported wiki needs only a local file server, including its Markdown and JSON mirrors.
import fs from 'node:fs';
import http from 'node:http';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const types = {
  '.html': 'text/html; charset=utf-8',
  '.md': 'text/markdown; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.txt': 'text/plain; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.xml': 'application/xml; charset=utf-8',
  '.svg': 'image/svg+xml', '.png': 'image/png', '.ico': 'image/x-icon',
  '.woff': 'font/woff', '.woff2': 'font/woff2',
};

export function createPreviewServer(directory) {
  const root = fs.realpathSync(directory);
  return http.createServer((request, response) => {
    const reply = (status, body) => {
      response.writeHead(status, { 'Content-Type': 'text/plain; charset=utf-8' });
      response.end(request.method === 'HEAD' ? undefined : body);
    };
    if (!['GET', 'HEAD'].includes(request.method)) {
      response.setHeader('Allow', 'GET, HEAD');
      return reply(405, 'Method not allowed');
    }
    let pathname;
    try {
      pathname = decodeURIComponent(new URL(request.url, 'http://localhost').pathname);
    } catch {
      return reply(400, 'Invalid URL');
    }
    if (pathname.includes('\0')) return reply(400, 'Invalid URL');
    if (pathname.replaceAll('\\', '/').split('/').includes('..')) return reply(403, 'Forbidden');
    let file = path.resolve(root, '.' + pathname);
    const inside = (candidate) => {
      const relative = path.relative(root, candidate);
      return relative !== '..' && !relative.startsWith('..' + path.sep) && !path.isAbsolute(relative);
    };
    if (!inside(file)) return reply(403, 'Forbidden');
    try {
      if (fs.statSync(file).isDirectory()) {
        if (!pathname.endsWith('/')) {
          const url = new URL(request.url, 'http://localhost');
          response.writeHead(308, { Location: url.pathname + '/' + url.search });
          return response.end();
        }
        file = path.join(file, 'index.html');
      }
    } catch { /* A missing route is handled by the exported 404 below. */ }
    let status = 200;
    if (!fs.existsSync(file)) {
      status = 404;
      file = path.join(root, '404.html');
      if (!fs.existsSync(file)) return reply(404, 'Not found');
    }
    try {
      if (!inside(fs.realpathSync(file))) return reply(403, 'Forbidden');
      const data = fs.readFileSync(file);
      response.writeHead(status, {
        'Content-Type': types[path.extname(file)] || 'application/octet-stream',
        'Content-Length': data.length,
        'Cache-Control': 'no-cache',
        'X-Content-Type-Options': 'nosniff',
      });
      response.end(request.method === 'HEAD' ? undefined : data);
    } catch {
      reply(404, 'Not found');
    }
  });
}

if (process.argv[1] && import.meta.url === pathToFileURL(path.resolve(process.argv[1])).href) {
  const output = fileURLToPath(new URL('../out/', import.meta.url));
  const portArg = process.argv.indexOf('--port');
  const port = Number(portArg >= 0 ? process.argv[portArg + 1] : process.env.PORT || 3000);
  if (!Number.isInteger(port) || port < 1 || port > 65535) {
    console.error('Use --port with a number from 1 to 65535.');
    process.exit(1);
  }
  if (!fs.existsSync(path.join(output, 'index.html'))) {
    console.error('Wiki/out is missing. Run npm run build before npm start.');
    process.exit(1);
  }
  const server = createPreviewServer(output);
  server.on('error', (error) => {
    console.error(`Preview failed: ${error.message}. Try npm start -- --port 3001.`);
    process.exitCode = 1;
  });
  server.listen(port, '127.0.0.1', () => {
    console.log(`AliceEngine wiki: http://127.0.0.1:${port}`);
    console.log('Serving Wiki/out. Rebuild after edits, or use npm run dev for live updates.');
  });
}
