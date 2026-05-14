import { expect, test } from '@playwright/test';

test('runs the README model with and without the worker bridge', async ({ page }) => {
  const browserErrors: string[] = [];
  let failOnPageError: (error: Error) => void = () => {};
  const pageErrorPromise = new Promise<never>((_, reject) => {
    failOnPageError = reject;
  });

  page.on('console', (message) => {
    if (message.type() === 'error') {
      browserErrors.push(`console error: ${message.text()}`);
    }
  });
  page.on('pageerror', (error) => {
    browserErrors.push(`page error: ${error.message}`);
    failOnPageError(error);
  });
  page.on('requestfailed', (request) => {
    browserErrors.push(`request failed: ${request.method()} ${request.url()} ${request.failure()?.errorText}`);
  });
  page.on('response', (response) => {
    if (response.status() >= 400) {
      browserErrors.push(`bad response: ${response.status()} ${response.url()}`);
    }
  });

  await page.goto('/');

  const status = page.locator('#status');
  try {
    await Promise.race([
      page.waitForFunction(() => {
        const text = document.getElementById('status')?.textContent;
        if (!text || text === 'pending') return false;
        const status = JSON.parse(text) as { ok?: boolean };
        return status.ok === true;
      }),
      pageErrorPromise,
    ]);
  } catch (error) {
    const statusText = await page
      .locator('#status')
      .evaluate((element) => element.textContent)
      .catch(() => '<missing #status>');
    throw new Error(
      [
        error instanceof Error ? error.message : String(error),
        `Current #status: ${statusText}`,
        ...browserErrors,
      ].join('\n\n'),
    );
  }

  const parsedStatus = JSON.parse(await status.textContent() ?? '{}') as {
    results?: Array<{
      mode?: string;
      ok?: boolean;
      solverStatus?: string;
      workerStats?: {
        total?: number;
        pthread?: number;
      };
    }>;
  };
  expect(parsedStatus.results).toHaveLength(2);
  expect(parsedStatus.results).toEqual([
    expect.objectContaining({ mode: 'direct', ok: true }),
    expect.objectContaining({ mode: 'worker', ok: true }),
  ]);
  expect(parsedStatus.results?.[0].workerStats).toEqual(
    expect.objectContaining({
      total: 2,
      pthread: 2,
    }),
  );
  expect(parsedStatus.results?.[1].workerStats?.total).toBeGreaterThanOrEqual(3);
});
