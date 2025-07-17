export default async function handler(req, res) {
  // Enable CORS
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type, Authorization');
  
  // Handle preflight requests
  if (req.method === 'OPTIONS') {
    res.status(200).end();
    return;
  }

  const ESP32_BASE_URL = 'http://10.17.192.136';
  
  try {
    // Get the path from query parameters, default to root
    const { path = '' } = req.query;
    
    // Construct the full URL to ESP32
    const targetUrl = path ? `${ESP32_BASE_URL}/${path}` : ESP32_BASE_URL;
    
    console.log(`Proxying ${req.method} request to: ${targetUrl}`);
    
    // Prepare request options
    const requestOptions = {
      method: req.method,
      headers: {
        'User-Agent': 'Vercel-Proxy/1.0',
        'Accept': '*/*',
      },
      timeout: 10000, // 10 second timeout
    };
    
    // Add body for POST/PUT requests
    if (req.method !== 'GET' && req.method !== 'HEAD' && req.body) {
      requestOptions.body = JSON.stringify(req.body);
      requestOptions.headers['Content-Type'] = 'application/json';
    }
    
    // Make request to ESP32
    const response = await fetch(targetUrl, requestOptions);
    
    if (!response.ok) {
      throw new Error(`ESP32 responded with status: ${response.status}`);
    }
    
    // Get content type
    const contentType = response.headers.get('content-type') || 'text/plain';
    
    // Set response headers
    res.setHeader('Content-Type', contentType);
    
    // Handle different content types
    if (contentType.includes('application/json')) {
      const data = await response.json();
      res.status(response.status).json(data);
    } else if (contentType.includes('text/html')) {
      const html = await response.text();
      // Modify HTML to fix relative paths if needed
      const modifiedHtml = html.replace(
        /src="\/([^"]+)"/g, 
        `src="/api/appliance?path=$1"`
      ).replace(
        /href="\/([^"]+)"/g, 
        `href="/api/appliance?path=$1"`
      );
      res.status(response.status).send(modifiedHtml);
    } else if (contentType.includes('image/')) {
      // Handle images
      const buffer = await response.arrayBuffer();
      res.setHeader('Content-Length', buffer.byteLength);
      res.status(response.status).send(Buffer.from(buffer));
    } else {
      // Handle other content types as text
      const text = await response.text();
      res.status(response.status).send(text);
    }
    
  } catch (error) {
    console.error('Proxy error:', error.message);
    
    // Check if it's a network error
    if (error.message.includes('fetch')) {
      res.status(503).json({ 
        error: 'ESP32 device not reachable',
        message: 'Make sure the ESP32 device at 10.17.192.136 is online and connected to the same network.',
        timestamp: new Date().toISOString()
      });
    } else {
      res.status(500).json({ 
        error: 'Proxy server error',
        message: error.message,
        timestamp: new Date().toISOString()
      });
    }
  }
} 