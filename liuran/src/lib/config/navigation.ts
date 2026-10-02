// hrefs use SvelteKit's `resolve` path form: base-relative with no leading slash
// (a leading slash would mean a route ID), except the root, which is '/'.
export const navItems = [
	{ href: '/', label: 'about' },
	{ href: 'publications/', label: 'publications' },
	{ href: 'software/', label: 'software' },
	{ href: 'essays/', label: 'essays' }
] as const;

// The absolute URL pathname a nav href is served at ('publications/' ->
// '/publications/'), i.e. what `page.url.pathname` reads on that page.
export const toPathname = (href: string) => (href === '/' ? href : `/${href}`);
