#!/usr/bin/env python3
"""Opens the WebAssembly demo page in Chromium and saves a screenshot.

Usage: capture_web.py <folder with index.html> <output.png>

Needs `pip install playwright` and `playwright install chromium`. The page
sets an element with id "ready" once the module has loaded.

This file is identical in every WFC library (wfc-vbrun, wfc-mscomctl,
wfc-comdlg32, wfc-mswinsck); a WFC test fails when the copies differ.
"""
import functools
import http.server
import sys
import threading

from playwright.sync_api import sync_playwright


def main():
    folder, output = sys.argv[1], sys.argv[2]
    handler = functools.partial(http.server.SimpleHTTPRequestHandler,
                                directory=folder)
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    url = "http://127.0.0.1:%d/index.html" % server.server_address[1]
    with sync_playwright() as playwright:
        browser = playwright.chromium.launch()
        page = browser.new_page(viewport={"width": 960, "height": 720})
        page.on("console", lambda message: print("console:", message.text))
        page.on("pageerror", lambda error: print("page error:", error))
        page.on("response", lambda response: print(response.status, response.url))
        page.goto(url)
        page.wait_for_selector("#ready", state="attached", timeout=30000)
        page.screenshot(path=output, full_page=True)
        browser.close()
    server.shutdown()


if __name__ == "__main__":
    main()
