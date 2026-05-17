import fs from 'fs';
import path from 'path';

const DEMO_TREE = {
  name: 'DemoProject',
  path: '/DemoProject',
  type: 'directory',
  hasChildren: true,
  children: [
    {
      name: 'src',
      path: '/DemoProject/src',
      type: 'directory',
      hasChildren: true,
      children: [
        { name: 'main.cpp', path: '/DemoProject/src/main.cpp', type: 'file' },
        { name: 'utils.h', path: '/DemoProject/src/utils.h', type: 'file' },
        {
          name: 'components',
          path: '/DemoProject/src/components',
          type: 'directory',
          hasChildren: true,
          children: [
            { name: 'Button.tsx', path: '/DemoProject/src/components/Button.tsx', type: 'file' },
            { name: 'Modal.tsx', path: '/DemoProject/src/components/Modal.tsx', type: 'file' }
          ]
        }
      ]
    },
    {
      name: 'tests',
      path: '/DemoProject/tests',
      type: 'directory',
      hasChildren: true,
      children: [
        { name: 'app.test.js', path: '/DemoProject/tests/app.test.js', type: 'file' },
        { name: 'utils.test.js', path: '/DemoProject/tests/utils.test.js', type: 'file' }
      ]
    },
    { name: 'package.json', path: '/DemoProject/package.json', type: 'file' },
    { name: 'README.md', path: '/DemoProject/README.md', type: 'file' },
    { name: '.gitignore', path: '/DemoProject/.gitignore', type: 'file' },
    {
      name: 'docs',
      path: '/DemoProject/docs',
      type: 'directory',
      hasChildren: true,
      children: [
        { name: 'api.md', path: '/DemoProject/docs/api.md', type: 'file' },
        { name: 'guide.md', path: '/DemoProject/docs/guide.md', type: 'file' }
      ]
    }
  ]
};

function findNode(node, targetPath) {
  if (node.path === targetPath) return node;
  if (!node.children) return null;
  for (const child of node.children) {
    const found = findNode(child, targetPath);
    if (found) return found;
  }
  return null;
}

export default function handler(req, res) {
  const { path: reqPath, show_hidden } = req.query;

  if (req.method !== 'GET') {
    return res.status(405).json({ error: 'Method not allowed' });
  }

  if (!reqPath) {
    return res.status(200).json(DEMO_TREE);
  }

  const node = findNode(DEMO_TREE, reqPath);
  
  if (!node) {
    return res.status(404).json({ error: 'Path not found', children: [] });
  }

  const showHidden = show_hidden === 'true' || show_hidden === '1';
  let children = node.children || [];

  if (!showHidden) {
    children = children.filter(c => !c.name.startsWith('.'));
  }

  return res.status(200).json({
    name: node.name,
    path: node.path,
    type: node.type || 'directory',
    hasChildren: children.length > 0,
    children: children
  });
}
