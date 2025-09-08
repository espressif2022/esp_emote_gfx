#!/bin/bash

# Deploy to Netlify
# Requires: npm install -g netlify-cli

echo "🌐 Deploying to Netlify..."

# Generate documentation
make -f Makefile.docs docs

# Deploy to Netlify (requires login: netlify login)
netlify deploy --prod --dir=docs/html/

echo "✅ Deployed to Netlify!"
