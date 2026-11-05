(() => {
  'use strict';

  /**
   * Compara la selección con la clave como conjuntos de identificadores.
   * Una clave vacía nunca puede considerarse un acierto.
   */
  function gradeQuestion(selectedIds, correctIds) {
    const selected = new Set(selectedIds);
    const correct = new Set(correctIds);
    return correct.size > 0 && selected.size === correct.size &&
      [...correct].every(id => selected.has(id));
  }

  const initialized = new WeakSet();

  function initQuiz(form) {
    if (!form || initialized.has(form)) return;
    initialized.add(form);

    form.addEventListener('submit', event => {
      event.preventDefault();

      const questions = [...form.querySelectorAll('[data-question]')];
      let hits = 0;

      for (const question of questions) {
        const boxes = [...question.querySelectorAll('input[type="checkbox"]')];
        const selectedIds = boxes.filter(box => box.checked).map(box => box.id);
        const correctIds = boxes
          .filter(box => box.dataset.correct === 'true')
          .map(box => box.id);

        const isCorrect = gradeQuestion(selectedIds, correctIds);
        if (isCorrect) hits++;

        const verdict = question.querySelector('[data-verdict]');
        const feedback = question.querySelector('[data-feedback]');
        if (verdict) {
          verdict.textContent = isCorrect ? 'Correcto.' : 'Revisá tu selección.';
        }
        if (feedback) feedback.hidden = false;
      }

      const summary = form.querySelector('[data-quiz-summary]');
      if (summary) {
        summary.textContent = `Acertaste ${hits} de ${questions.length} preguntas.`;
      }
    });

    form.addEventListener('change', () => {
      for (const feedback of form.querySelectorAll('[data-feedback]')) {
        feedback.hidden = true;
      }
      const summary = form.querySelector('[data-quiz-summary]');
      if (summary) summary.textContent = '';
    });
  }

  function initAll() {
    document.querySelectorAll('form[data-quiz]').forEach(initQuiz);
  }

  window.TP3Quiz = { gradeQuestion, initQuiz };

  // Los scripts defer se ejecutan con el DOM ya parseado; el listener
  // cubre además el caso de una carga anticipada.
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initAll);
  } else {
    initAll();
  }
})();
