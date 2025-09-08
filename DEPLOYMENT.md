# 📚 ESP Emote GFX Documentation Deployment Guide

This guide covers multiple ways to publish your API documentation online.

## 🚀 Quick Start

### Generate Documentation
```bash
# Generate all documentation
make -f Makefile.docs docs
```

## 🌐 Deployment Options

### 1. GitHub Pages (Recommended)

#### Automatic Deployment
The GitHub Actions workflow will automatically deploy docs when you push to `main` branch.

**Setup:**
1. Push your code to GitHub
2. Go to **Repository Settings** → **Pages**
3. Select **Deploy from a branch** 
4. Choose **gh-pages** branch
5. Select **/ (root)** folder
6. Click **Save**

**Access:** `https://YOUR_USERNAME.github.io/YOUR_REPOSITORY/api-docs/`

#### Manual Deployment
```bash
./scripts/deploy-docs.sh
```

### 2. Netlify

**Requirements:** `npm install -g netlify-cli`

```bash
# Login to Netlify
netlify login

# Deploy
./scripts/deploy-netlify.sh
```

**Features:**
- ✅ Custom domains
- ✅ Form handling
- ✅ Edge functions
- ✅ Branch previews

### 3. Vercel

**Requirements:** `npm install -g vercel`

```bash
# Login to Vercel
vercel login

# Deploy
./scripts/deploy-vercel.sh
```

**Features:**
- ⚡ Ultra-fast CDN
- ✅ Custom domains
- ✅ Analytics
- ✅ Preview deployments

### 4. Self-Hosted

#### Apache/Nginx
```bash
# Generate docs
make -f Makefile.docs docs

# Copy to web server
scp -r docs/html/* user@your-server.com:/var/www/html/docs/
```

#### Docker
```dockerfile
FROM nginx:alpine
COPY docs/html /usr/share/nginx/html
EXPOSE 80
```

```bash
# Build and run
docker build -t esp-emote-docs .
docker run -p 8080:80 esp-emote-docs
```

## 🔧 Advanced Configuration

### Custom Domain (GitHub Pages)
1. Add `CNAME` file to docs:
   ```bash
   echo "docs.yourdomain.com" > docs/html/CNAME
   ```
2. Configure DNS A records to point to GitHub's servers

### CI/CD Integration

#### GitLab CI
```yaml
# .gitlab-ci.yml
pages:
  script:
    - apt-get update && apt-get install -y doxygen python3
    - make -f Makefile.docs docs
    - mv docs/html public
  artifacts:
    paths:
      - public
  only:
    - main
```

#### Jenkins
```groovy
pipeline {
    agent any
    stages {
        stage('Build Docs') {
            steps {
                sh 'make -f Makefile.docs docs'
            }
        }
        stage('Deploy') {
            steps {
                publishHTML([
                    allowMissing: false,
                    alwaysLinkToLastBuild: true,
                    keepAll: true,
                    reportDir: 'docs/html',
                    reportFiles: 'index.html',
                    reportName: 'API Documentation'
                ])
            }
        }
    }
}
```

## 📱 Mobile Optimization

The generated Doxygen documentation is mobile-responsive by default, but you can customize:

```css
/* Custom CSS in Doxyfile */
HTML_EXTRA_STYLESHEET = custom.css
```

## 🔍 SEO Optimization

### Meta Tags
Add to your Doxygen header:
```html
<meta name="description" content="ESP Emote GFX API Documentation">
<meta name="keywords" content="ESP32, Graphics, API, Documentation">
```

### Sitemap
```bash
# Generate sitemap for better SEO
find docs/html -name "*.html" | sed 's|docs/html|https://yoursite.com|g' > sitemap.txt
```

## 📊 Analytics

### Google Analytics
Add to Doxygen footer:
```html
<!-- Google Analytics -->
<script async src="https://www.googletagmanager.com/gtag/js?id=GA_MEASUREMENT_ID"></script>
<script>
  window.dataLayer = window.dataLayer || [];
  function gtag(){dataLayer.push(arguments);}
  gtag('js', new Date());
  gtag('config', 'GA_MEASUREMENT_ID');
</script>
```

## 🚨 Troubleshooting

### Common Issues

**GraphViz errors during generation:**
```bash
# Install missing dependencies
sudo apt-get install graphviz
```

**GitHub Pages not updating:**
- Check Actions tab for build errors
- Verify gh-pages branch exists
- Clear browser cache

**Large documentation size:**
- Optimize images: `optipng docs/html/*.png`
- Enable compression in web server
- Use CDN for static assets

## 📈 Performance Tips

1. **Enable compression** on your web server
2. **Use CDN** for static assets
3. **Optimize images** in documentation
4. **Enable caching** headers
5. **Minify CSS/JS** if using custom styles

## 🔒 Access Control

### Private Documentation
For internal APIs, consider:
- **Basic Auth** on web server
- **VPN-only access**
- **GitHub private pages** (Enterprise)
- **Password-protected Netlify sites**
