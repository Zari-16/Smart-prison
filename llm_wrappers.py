"""
LLM wrapper templates for safe synthetic data generation and assisted labeling.
Supports OpenAI, Qwen, Anthropic (templates).
Store API keys in environment variables: OPENAI_API_KEY, QWEN_API_KEY, CLAUDE_API_KEY
Only use LLMs for synthetic generation and label suggestions (human-in-loop).
"""
import os, requests, json

def call_openai(prompt, model="gpt-4o-mini", max_tokens=200):
    key = os.environ.get("OPENAI_API_KEY")
    if not key: raise RuntimeError("OPENAI_API_KEY not set")
    url = "https://api.openai.com/v1/chat/completions"
    headers = {"Authorization": f"Bearer {key}", "Content-Type":"application/json"}
    payload = {"model": model, "messages":[{"role":"user","content":prompt}], "max_tokens": max_tokens}
    r = requests.post(url, headers=headers, json=payload, timeout=30)
    return r.json()

def call_qwen(prompt, model="qwen-3-pro", max_tokens=512):
    key = os.environ.get("QWEN_API_KEY")
    if not key: raise RuntimeError("QWEN_API_KEY not set")
    # placeholder - consult provider docs for actual endpoint / headers
    url = "https://api.example-qwen.com/v1/chat/completions"
    headers = {"Authorization": f"Bearer {key}", "Content-Type":"application/json"}
    payload = {"model": model, "messages":[{"role":"user","content":prompt}], "max_tokens": max_tokens}
    r = requests.post(url, headers=headers, json=payload, timeout=30)
    return r.json()

def call_claude(prompt, model="claude-2", max_tokens=512):
    key = os.environ.get("CLAUDE_API_KEY")
    if not key: raise RuntimeError("CLAUDE_API_KEY not set")
    url = "https://api.anthropic.com/v1/complete"
    headers = {"x-api-key": key, "Content-Type":"application/json"}
    payload = {"model": model, "prompt": prompt, "max_tokens_to_sample": max_tokens}
    r = requests.post(url, headers=headers, json=payload, timeout=30)
    return r.json()
