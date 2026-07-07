import { describe, it, expect } from 'vitest';
import { sanitizeHtml } from './sanitize';

describe('sanitizeHtml', () => {
	it('preserves the inline formatting used by titles and author lists', () => {
		expect(sanitizeHtml('<i>King Lear</i> and <b>Ran Liu</b>')).toBe(
			'<i>King Lear</i> and <b>Ran Liu</b>'
		);
	});

	it('preserves rendered-markdown structure', () => {
		const html = '<h2>Heading</h2>\n<p>A <a href="https://example.com">link</a>.</p>';
		expect(sanitizeHtml(html)).toBe(html);
	});

	it('strips script tags', () => {
		expect(sanitizeHtml('safe<script>alert(1)</script>')).toBe('safe');
	});

	it('strips event-handler attributes', () => {
		expect(sanitizeHtml('<img src="x" onerror="alert(1)" />')).toBe('<img src="x">');
	});

	it('strips javascript: URLs', () => {
		expect(sanitizeHtml('<a href="javascript:alert(1)">x</a>')).toBe('<a>x</a>');
	});
});
