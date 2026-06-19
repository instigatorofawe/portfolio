// See https://svelte.dev/docs/kit/types#app.d.ts
// for information about these interfaces
declare global {
	namespace App {
		// interface Error {}
		// interface Locals {}
		// interface PageData {}
		// interface PageState {}
		// interface Platform {}
	}

	// Latest git commit date (ISO 8601), injected at build time via vite.config.ts.
	const __LAST_COMMIT_DATE__: string;
}

export {};
