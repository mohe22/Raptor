const API_BASE_URL = "http://localhost:8000";
export async function api<T>(url: string, options?: RequestInit): Promise<T> {
  const res = await fetch(API_BASE_URL + url, {
    credentials: "include",
    headers: {
      "Content-Type": "application/json",
      ...(options?.headers || {}),
    },
    ...options,
  });
  console.log(res);
  if (!res.ok) {
    const text = await res.text();
    let message = "Request failed";
    try {
      const json = JSON.parse(text);
      message = json.message || json.error || message;
    } catch {
      message = text || message;
    }
    throw new Error(message);
  }

  return res.json();
}
