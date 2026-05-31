"use strict";

const fs = require('fs');
const path = require('path');

function setupFetchShim() {
  if (typeof process !== 'undefined' && process.versions && process.versions.node && typeof fetch === 'function') {
    const originalFetch = fetch;
    global.fetch = async (resource, init) => {
      if (typeof resource === 'string' && resource.startsWith('/')) {
        const requestedPath = path.resolve(resource);
        const expectedWasmPath = path.resolve(__dirname, 'ffish.wasm');
        if (requestedPath === expectedWasmPath) {
          try {
            const bytes = await fs.promises.readFile(expectedWasmPath);
            return new Response(bytes, { status: 200 });
          } catch (err) {
            if (err.code === 'ENOENT') {
              return new Response(null, { status: 404 });
            }
            console.error(`Unexpected filesystem error loading ${resource}:`, err);
            throw err;
          }
        } else {
          console.warn(`Blocked unexpected absolute path fetch request for: ${resource}`);
          return new Response(null, { status: 403 });
        }
      }
      return originalFetch(resource, init);
    };
  }
}

module.exports = { setupFetchShim };
