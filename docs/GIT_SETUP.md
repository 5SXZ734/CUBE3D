# Git Setup Instructions

## Step 1: Initialize Git Repository (in your project directory)

```bash
cd /path/to/your/cube_viewer
git init
```

## Step 2: Add files

```bash
# Add all source files
git add *.cpp *.h CMakeLists.txt README.md .gitignore

# Or add everything (will respect .gitignore)
git add .
```

## Step 3: Create initial commit

```bash
git commit -m "Initial commit: Multi-API 3D renderer with OpenGL, D3D11, D3D12"
```

## Step 4: Create GitHub repository

Go to https://github.com/new and create a new repository:
- Repository name: `cube_viewer` (or whatever you prefer)
- Description: "Cross-platform 3D renderer supporting OpenGL, DirectX 11, and DirectX 12"
- Public or Private: Your choice
- **DO NOT** initialize with README, .gitignore, or license (we already have these)

## Step 5: Link to GitHub

```bash
# Replace 'yourusername' with your GitHub username
git remote add origin https://github.com/yourusername/cube_viewer.git

# Or if using SSH:
git remote add origin git@github.com:yourusername/cube_viewer.git
```

## Step 6: Push to GitHub

```bash
# For first push
git branch -M main
git push -u origin main

# For subsequent pushes
git push
```

## Alternative: GitHub Desktop

If you prefer a GUI:

1. Download GitHub Desktop: https://desktop.github.com/
2. Open GitHub Desktop
3. File → Add Local Repository
4. Select your `cube_viewer` folder
5. Click "Publish repository"
6. Choose public/private and click "Publish Repository"

## Handling External Dependencies

### Option A: Git Submodules (Recommended)

If you want external/glfw and external/glad in your repo:

```bash
cd ..  # Go to parent directory
git init  # If not already a git repo

# Add GLFW as submodule
git submodule add https://github.com/glfw/glfw.git external/glfw

# For GLAD, you'll need to commit it directly since it's generated
git add external/glad
git commit -m "Add GLAD OpenGL loader"
```

### Option B: Document in README

Don't commit external dependencies, just document how to get them:

```markdown
## Setup External Dependencies

1. Clone GLFW:
   ```bash
   cd ..
   git clone https://github.com/glfw/glfw.git external/glfw
   ```

2. Generate GLAD from https://glad.dav1d.de/ and place in ../external/glad/
```

This keeps your repo smaller.

## Common Git Commands

```bash
# Check status
git status

# Add specific files
git add file1.cpp file2.h

# Commit changes
git commit -m "Description of changes"

# Push to GitHub
git push

# Pull latest changes
git pull

# Create a new branch
git checkout -b feature-name

# Switch branches
git checkout main

# View commit history
git log --oneline
```

## .gitignore is Important!

The .gitignore file prevents build artifacts from being committed:
- build/ directory
- Visual Studio files
- Compiled binaries
- Object files

This keeps your repo clean and small.

## Typical Workflow

```bash
# 1. Make changes to your code
vim renderer_opengl.cpp

# 2. Check what changed
git status
git diff

# 3. Stage changes
git add renderer_opengl.cpp

# 4. Commit
git commit -m "Fix OpenGL depth buffer issue"

# 5. Push to GitHub
git push
```

## Creating Releases

Once your code is stable:

1. Go to your GitHub repo
2. Click "Releases" → "Create a new release"
3. Tag version: `v1.0.0`
4. Release title: "Initial Release"
5. Describe features
6. Attach compiled binaries (optional)
7. Publish release

## Tips

- Commit often with clear messages
- Use branches for new features
- Write good commit messages: "Fix X" not "Updated stuff"
- Don't commit build artifacts
- Keep README.md updated
- Add screenshots/GIFs to showcase your project
