#!/bin/bash

# ESP Emote GFX Documentation Deployment Script

set -e

echo "🚀 Starting documentation deployment..."

# Generate fresh documentation
echo "📚 Generating documentation..."
make -f Makefile.docs docs

# Check if gh-pages branch exists
if git show-ref --verify --quiet refs/heads/gh-pages; then
    echo "✅ gh-pages branch exists"
else
    echo "🔧 Creating gh-pages branch..."
    git checkout --orphan gh-pages
    git rm -rf .
    git commit --allow-empty -m "Initial gh-pages commit"
    git checkout -
fi

# Save current branch
CURRENT_BRANCH=$(git branch --show-current)

# Switch to gh-pages
echo "🔄 Switching to gh-pages branch..."
git checkout gh-pages

# Clear existing docs (keep .git and important files)
find . -maxdepth 1 ! -name '.git' ! -name '.' ! -name '..' -exec rm -rf {} + 2>/dev/null || true

# Copy generated documentation
echo "📋 Copying documentation files..."
git checkout $CURRENT_BRANCH -- docs/html
cp -r docs/html/* .
rm -rf docs/

# Create index page if it doesn't exist
if [ ! -f index.html ]; then
    echo "<!DOCTYPE html>
<html>
<head>
    <meta charset=\"utf-8\">
    <title>ESP Emote GFX Documentation</title>
    <meta http-equiv=\"refresh\" content=\"0; url=./index.html\">
</head>
<body>
    <p>If you are not redirected automatically, <a href=\"./index.html\">click here</a>.</p>
</body>
</html>" > index.html
fi

# Add CNAME for custom domain (optional)
# echo "your-domain.com" > CNAME

# Add all files and commit
git add .
git commit -m "📚 Update documentation $(date '+%Y-%m-%d %H:%M:%S')"

# Push to GitHub
echo "🚀 Pushing to GitHub Pages..."
git push origin gh-pages

# Switch back to original branch
git checkout $CURRENT_BRANCH

echo "✅ Documentation deployed successfully!"
echo "📖 Your documentation will be available at:"
echo "   https://YOUR_USERNAME.github.io/YOUR_REPOSITORY/"
echo ""
echo "💡 To enable GitHub Pages:"
echo "   1. Go to your repository settings"
echo "   2. Scroll to 'Pages' section" 
echo "   3. Select 'Deploy from a branch'"
echo "   4. Choose 'gh-pages' branch"
echo "   5. Select '/ (root)' folder"
