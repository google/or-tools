import { defineConfig, devices } from '@playwright/test';

export default defineConfig({
  testDir: './specs',
  timeout: 120_000,
  expect: {
    timeout: 120_000,
  },
  webServer: [
    {
      command: 'npm run test:fixture:vite:serve-dev',
      url: 'http://127.0.0.1:4174',
      reuseExistingServer: !process.env.CI,
      timeout: 120_000,
    },
    {
      command: 'npm run test:fixture:vite:serve-static',
      url: 'http://127.0.0.1:4175',
      reuseExistingServer: !process.env.CI,
      timeout: 120_000,
    },
  ],
  projects: [
    {
      name: 'vite-dev-chromium',
      use: {
        ...devices['Desktop Chrome'],
        baseURL: 'http://127.0.0.1:4174',
      },
    },
    {
      name: 'vite-dev-firefox',
      use: {
        ...devices['Desktop Firefox'],
        baseURL: 'http://127.0.0.1:4174',
      },
    },
    {
      name: 'vite-static-chromium',
      use: {
        ...devices['Desktop Chrome'],
        baseURL: 'http://127.0.0.1:4175',
      },
    },
    {
      name: 'vite-static-firefox',
      use: {
        ...devices['Desktop Firefox'],
        baseURL: 'http://127.0.0.1:4175',
      },
    },
  ],
});
