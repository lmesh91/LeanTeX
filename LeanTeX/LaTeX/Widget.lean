import Lean.Widget

namespace LeanTeX
namespace LaTeX

open Lean

structure LatexWidgetProps where
  latex : String
  deriving ToJson, FromJson, Server.RpcEncodable

-- The widget renders the LaTeX string through MathJax when the infoview can load it,
-- and reports an error message if the renderer fails to load.
@[widget_module]
def latexWidget : Lean.Widget.Module where
  javascript := "
import * as React from 'react';

const MATHJAX_CDN = 'https://cdn.jsdelivr.net/npm/mathjax@4/tex-svg.js';

function ensureMathJax() {
  if (window.LeanTeXMathJaxPromise) {
    return window.LeanTeXMathJaxPromise;
  }

  if (!window.MathJax) {
    window.MathJax = {
      tex: {
        packages: { '[+]': ['ams'] },
      },
      svg: {
        fontCache: 'local',
      },
      startup: {
        typeset: false,
      },
    };
  }

  window.LeanTeXMathJaxPromise = new Promise((resolve, reject) => {
    if (window.MathJax?.tex2svgPromise) {
      resolve(window.MathJax);
      return;
    }

    const script = document.createElement('script');
    script.src = MATHJAX_CDN;
    script.async = true;
    script.onload = () => resolve(window.MathJax);
    script.onerror = () => reject(new Error('Failed to load MathJax.'));
    document.head.appendChild(script);
  }).then(async (mathjax) => {
    if (mathjax?.startup?.promise) {
      await mathjax.startup.promise;
    }
    return mathjax;
  });

  return window.LeanTeXMathJaxPromise;
}

export default function (props) {
  const latex = typeof props.latex === 'string' ? props.latex : '';
  const previewRef = React.useRef(null);
  const [error, setError] = React.useState('');

  React.useEffect(() => {
    let cancelled = false;

    async function renderLatex() {
      const preview = previewRef.current;
      if (!preview) {
        return;
      }

      if (!latex) {
        preview.replaceChildren();
        setError('');
        return;
      }

      setError('');

      try {
        const mathjax = await ensureMathJax();
        if (cancelled || !preview) {
          return;
        }
        const rendered = await mathjax.tex2svgPromise(latex, { display: true });
        if (cancelled || !preview) {
          return;
        }
        preview.replaceChildren(rendered);
      } catch (err) {
        if (cancelled || !preview) {
          return;
        }
        preview.replaceChildren();
        setError(err instanceof Error ? err.message : String(err));
      }
    }

    renderLatex();
    return () => {
      cancelled = true;
    };
  }, [latex]);

  return React.createElement(
    'div',
    {
      style: {},
    },
    [
      error ? React.createElement(
        'div',
        {
          key: 'error',
          style: {
            color: 'var(--vscode-errorForeground)',
          },
        },
        error
      ) : null,
      React.createElement(
        'div',
        {
          key: 'preview',
          ref: previewRef,
          style: {
            overflowX: 'auto',
          },
        }
      ),
    ]
  );
}
"

end LaTeX
end LeanTeX
