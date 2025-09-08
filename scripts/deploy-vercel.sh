#!/bin/bash

# Deploy to Vercel
# Requires: npm install -g vercel

echo "⚡ Deploying to Vercel..."

# Generate documentation
make -f Makefile.docs docs

# Create vercel.json if it doesn't exist
if [ ! -f vercel.json ]; then
    cat > vercel.json << EOF
{
  "version": 2,
  "name": "esp-emote-gfx-docs",
  "builds": [
    {
      "src": "docs/html/**",
      "use": "@vercel/static"
    }
  ],
  "routes": [
    {
      "src": "/(.*)",
      "dest": "/docs/html/\$1"
    }
  ]
}
EOF
fi

# Deploy to Vercel (requires login: vercel login)
vercel --prod

echo "✅ Deployed to Vercel!"
