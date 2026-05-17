const DEFAULT_SETTINGS = {
  showHidden: false,
  enableHW: false,
  language: 'zh',
  theme: 'mocha',
  mode: 'light',
  fontSize: 14,
  fontFamily: '"Cascadia Code","Fira Code","JetBrains Mono",monospace',
  animDuration: 200,
  animEnabled: true,
  enableJSMode: false
};

let settings = { ...DEFAULT_SETTINGS };

export default function handler(req, res) {
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type');

  if (req.method === 'OPTIONS') {
    return res.status(200).end();
  }

  if (req.method === 'GET') {
    return res.status(200).json(settings);
  }

  if (req.method === 'POST') {
    try {
      const body = typeof req.body === 'string' ? JSON.parse(req.body) : req.body;
      settings = { ...settings, ...body };
      return res.status(200).json({ success: true, settings });
    } catch (e) {
      return res.status(400).json({ error: 'Invalid JSON' });
    }
  }

  return res.status(405).json({ error: 'Method not allowed' });
}
