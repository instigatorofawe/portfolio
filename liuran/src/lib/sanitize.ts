import DOMPurify from 'isomorphic-dompurify';

// Central sanitizer for every string the app injects via `{@html}` (rendered
// essay markdown, and the inline-HTML titles/authors in the YAML data files).
// All of that content is authored in this repo, so this is defense in depth:
// it keeps a future content source — or a compromised dependency emitting
// markup — from turning an `{@html}` site into an XSS vector.
//
// isomorphic-dompurify runs DOMPurify against jsdom during prerendering and
// against the real DOM in the browser, so the universal load functions can
// call this in both environments.
export function sanitizeHtml(html: string): string {
	return DOMPurify.sanitize(html);
}
