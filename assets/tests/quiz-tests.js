(() => {
  const results = document.querySelector('#results');
  const fixtures = document.querySelector('#fixtures');
  let failures = 0;
  function assert(value, message) {
    if (!value) throw new Error(message);
  }
  function test(name, fn) {
    try {
      fn();
      results.textContent += `PASS: ${name}\n`;
    } catch (error) {
      failures++;
      results.textContent += `FAIL: ${name}: ${error.message}\n`;
    }
  }
  function fixture() {
    const form = document.createElement('form');
    form.dataset.quiz = '';
    const prefix = `f${fixtures.children.length}`;
    form.innerHTML = `
      <fieldset data-question>
        <legend>Fixture uno</legend>
        <label><input id="${prefix}-a" type="checkbox" data-correct="true"> A</label>
        <label><input id="${prefix}-b" type="checkbox" data-correct="true"> B</label>
        <label><input id="${prefix}-c" type="checkbox" data-correct="false"> C</label>
        <div data-feedback hidden><p data-verdict></p><p>A y B son correctas porque este fixture exige ambas.</p></div>
      </fieldset>
      <fieldset data-question>
        <legend>Fixture dos</legend>
        <label><input id="${prefix}-d" type="checkbox" data-correct="true"> D</label>
        <label><input id="${prefix}-e" type="checkbox" data-correct="false"> E</label>
        <div data-feedback hidden><p data-verdict></p><p>D es correcta; E es un distractor.</p></div>
      </fieldset>
      <button type="submit">Comprobar respuestas</button>
      <p data-quiz-summary role="status"></p>`;
    fixtures.append(form);
    window.TP3Quiz.initQuiz(form);
    return form;
  }
  function submit(form) {
    form.dispatchEvent(new Event('submit', { bubbles: true, cancelable: true }));
  }
  test('API disponible', () => {
    assert(typeof window.TP3Quiz?.gradeQuestion === 'function', 'falta gradeQuestion');
    assert(typeof window.TP3Quiz?.initQuiz === 'function', 'falta initQuiz');
  });
  test('coincidencia exacta, orden independiente y distractores', () => {
    const grade = window.TP3Quiz.gradeQuestion;
    assert(grade(['a', 'b'], ['a', 'b']), 'conjunto exacto');
    assert(grade(['b', 'a'], ['a', 'b']), 'orden invertido');
    assert(!grade([], ['a', 'b']), 'ninguna selección');
    assert(!grade(['a'], ['a', 'b']), 'selección parcial');
    assert(!grade(['a', 'b', 'c'], ['a', 'b']), 'distractor extra');
    assert(!grade(['c'], ['a', 'b']), 'solo distractor');
    assert(grade(['a'], ['a']), 'una única correcta');
    assert(!grade([], []), 'una clave vacía no puede dar un acierto');
  });
  test('inicialmente no muestra explicaciones', () => {
    const form = fixture();
    assert([...form.querySelectorAll('[data-feedback]')].every(el => el.hidden), 'feedback inicial visible');
  });
  test('sin selección explica todas las preguntas y marca cero', () => {
    const form = fixture();
    submit(form);
    const feedback = [...form.querySelectorAll('[data-feedback]')];
    assert(feedback.every(el => !el.hidden), 'falta una explicación');
    assert(feedback.every(el => el.textContent.includes('correcta')), 'explicación ausente');
    assert(form.querySelector('[data-quiz-summary]').textContent === 'Acertaste 0 de 2 preguntas.', 'resumen incorrecto');
  });
  test('mezcla de acierto y fallo, cambio y segundo intento', () => {
    const form = fixture();
    const inputs = [...form.querySelectorAll('input')];
    inputs[0].checked = inputs[1].checked = true;
    submit(form);
    const verdicts = [...form.querySelectorAll('[data-verdict]')];
    assert(verdicts[0].textContent === 'Correcto.', 'no detecta acierto');
    assert(verdicts[1].textContent === 'Revisá tu selección.', 'no detecta fallo');
    assert([...form.querySelectorAll('[data-feedback]')].every(el => !el.hidden), 'falta feedback');
    inputs[2].checked = true;
    inputs[2].dispatchEvent(new Event('change', { bubbles: true }));
    assert(form.querySelector('[data-quiz-summary]').textContent === '', 'resumen obsoleto');
    assert([...form.querySelectorAll('[data-feedback]')].every(el => el.hidden), 'feedback obsoleto');
    submit(form);
    assert(verdicts[0].textContent === 'Revisá tu selección.', 'no penaliza distractor extra');
    inputs[2].checked = false;
    inputs[3].checked = true;
    submit(form);
    assert(form.querySelector('[data-quiz-summary]').textContent === 'Acertaste 2 de 2 preguntas.', 'no recalcula');
  });
  test('formularios aislados e inicialización idempotente', () => {
    const first = fixture();
    const second = fixture();
    window.TP3Quiz.initQuiz(first);
    submit(first);
    assert(second.querySelector('[data-quiz-summary]').textContent === '', 'otro formulario modificado');
    assert([...second.querySelectorAll('[data-feedback]')].every(el => el.hidden), 'otro feedback modificado');
    assert(first.querySelector('[data-quiz-summary]').textContent === 'Acertaste 0 de 2 preguntas.', 'resultado duplicado');
  });
  results.textContent += failures === 0 ? '\nTODAS LAS PRUEBAS PASARON.\n' : `\nFALLOS: ${failures}\n`;
})();
