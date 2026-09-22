import unittest
from convert_lunarisv_players import convert

class ConversionTests(unittest.TestCase):
    def fixture(self):
        return {'Config': {'-2': {'Name': 'Cheating/Cheater'}, '-3': {'Name': 'Friend'},
                           '-6': {'Name': 'Suspected Cheater'}, '1': {'Name': 'Pedofilia/Pedo Jokes'}},
                'Tags': {'123': ['-2', '-6', '-2', '-3'], '456': ['-3'], '789': ['1']},
                'LastSeenNames': {'123': 'Unicode \u2603 name'}}

    def test_conversion_filters_and_preserves(self):
        result, report = convert(self.fixture())
        self.assertEqual(result['players'], [
            {'steamid': '[U:1:123]', 'attributes': ['cheater', 'suspected_cheater'],
             'last_seen': {'player_name': 'Unicode \u2603 name', 'time': 0}},
            {'steamid': '[U:1:789]', 'attributes': ['pedophilia']}])
        self.assertEqual(report['players_without_allowed_tags_omitted'], 1)
        self.assertEqual(report['excluded_tag_assignments_removed'], 2)
        self.assertNotIn('update_url', result['file_info'])

    def test_undefined_tag_fails(self):
        data = self.fixture(); data['Tags']['123'] = ['unknown']
        with self.assertRaises(ValueError): convert(data)

    def test_bad_account_id_fails(self):
        for value in ['0', '-1', '4294967296']:
            data = self.fixture(); data['Tags'] = {value: ['-2']}
            with self.assertRaises(ValueError): convert(data)

    def test_unmapped_tag_fails(self):
        data = self.fixture(); data['Config']['x'] = {'Name': 'New tag'}
        with self.assertRaises(ValueError): convert(data)

if __name__ == '__main__':
    unittest.main()
