import { defineConfig, devices } from '@playwright/test';

const fixtureGroup = process.env.FIXTURE_GROUP;
const webServers = [
  {
    fixture: 'vite',
    command: 'npm run test:fixture:vite:serve-dev',
    url: 'http://127.0.0.1:4174',
    reuseExistingServer: !process.env.CI,
    timeout: 120_000,
  },
  {
    fixture: 'vite',
    command: 'npm run test:fixture:vite:serve-static',
    url: 'http://127.0.0.1:4175',
    reuseExistingServer: !process.env.CI,
    timeout: 120_000,
  },
  {
    fixture: 'webpack',
    command: 'npm run test:fixture:webpack:serve',
    url: 'http://127.0.0.1:4176',
    reuseExistingServer: !process.env.CI,
    timeout: 120_000,
  },
  {
    fixture: 'webpack',
    command: 'npm run test:fixture:webpack:serve-static',
    url: 'http://127.0.0.1:4177',
    reuseExistingServer: !process.env.CI,
    timeout: 120_000,
  },
]
  .filter((server) => !fixtureGroup || server.fixture === fixtureGroup)
  .map(({ fixture: _fixture, ...server }) => server);

export default defineConfig({
  testDir: './specs',
  timeout: 120_000,
  expect: {
    timeout: 120_000,
  },
  webServer: webServers,
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
    {
      name: 'webpack-dev-chromium',
      use: {
        ...devices['Desktop Chrome'],
        baseURL: 'http://127.0.0.1:4176',
      },
    },
    {
      name: 'webpack-dev-firefox',
      use: {
        ...devices['Desktop Firefox'],
        baseURL: 'http://127.0.0.1:4176',
      },
    },
    {
      name: 'webpack-static-chromium',
      use: {
        ...devices['Desktop Chrome'],
        baseURL: 'http://127.0.0.1:4177',
      },
    },
    {
      name: 'webpack-static-firefox',
      use: {
        ...devices['Desktop Firefox'],
        baseURL: 'http://127.0.0.1:4177',
      },
    },
  ],
});
